"""No Serial access: test helper using an in-memory transport."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch
import zlib

spec = importlib.util.spec_from_file_location('helper', Path(__file__).parents[1] / 'provisioning/provision_ble_key.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)


class Port:
    def __init__(self, response=b'OK verified\n'):
        self.lines = [b'BKP1 DEVICE 02:03:04:05:06:07\n', response]
        self.frame = b''

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def read_until(self, *args):
        return self.lines.pop(0)

    def write(self, data):
        self.frame = bytes(data)
        return len(data)

    def flush(self):
        pass


class Tests(unittest.TestCase):
    def run_cli(self, port, extra=(), input_error=None):
        output, errors = io.StringIO(), io.StringIO()
        secret = 'synthetic-private-fixture'
        with patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=lambda *a, **k: port)}), \
                patch.object(sys, 'argv', ['helper', '--port', 'FAKE', *extra]), \
                patch('builtins.input', return_value='02:03:04:05:06:07'), \
                patch.object(helper.getpass, 'getpass', return_value=secret, side_effect=input_error) as prompt, \
                contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = helper.cli()
        text = output.getvalue() + errors.getvalue()
        self.assertNotIn(secret, text)
        return code, text, prompt

    def test_diagnostic_responses(self):
        cases = list(helper.RESPONSES.items()) + [
            (b'', 'no-response'),
            (b'synthetic-private-fixture', 'unexpected-response'),
            (b'OK ver', 'unexpected-response'),
        ]
        for response, category in cases:
            with self.subTest(category=category):
                code, text, _ = self.run_cli(Port(response))
                self.assertEqual(code, 1)
                self.assertIn('Diagnostic: ' + category, text)

    def test_hidden_input_errors_never_write(self):
        for error in [EOFError('synthetic-private-fixture'), KeyboardInterrupt(),
                      helper.getpass.GetPassWarning('synthetic-private-fixture'),
                      UnicodeError('synthetic-private-fixture')]:
            with self.subTest(error=type(error).__name__):
                port = Port()
                code, text, _ = self.run_cli(port, input_error=error)
                self.assertEqual(code, 1)
                self.assertIn('hidden-input-error', text)
                self.assertEqual(port.frame, b'')

    def test_serial_errors_and_short_write(self):
        for stage in ['write', 'flush', 'read_until', '__exit__']:
            port = Port()
            # read_until fails after the identity, not before it.
            behavior = [port.lines[0], OSError('synthetic-private-fixture')] if stage == 'read_until' else OSError('synthetic-private-fixture')
            with patch.object(Port, stage, side_effect=behavior):
                code, text, _ = self.run_cli(port)
            self.assertEqual(code, 1)
            self.assertIn('serial-error', text)
        with patch.object(Port, 'write', return_value=0):
            code, text, _ = self.run_cli(Port())
        self.assertEqual(code, 1)
        self.assertIn('serial-error', text)

    def test_status_only_sends_query_without_prompt(self):
        for status in helper.STATUSES:
            port = Port(status)
            code, text, prompt = self.run_cli(port, ['--status'])
            self.assertEqual(code, 0)
            self.assertIn(status.decode('ascii'), text)
            prompt.assert_not_called()
            self.assertEqual(port.frame, b'BKS1')

    def test_status_rejects_old_firmware_and_unknown_output(self):
        for response in [b'ERROR frame', b'', b'synthetic-private-fixture', b'OK verified']:
            code, text, prompt = self.run_cli(Port(response), ['--status'])
            self.assertEqual(code, 1)
            prompt.assert_not_called()

    def test_backend_format_and_no_automatic_replace(self):
        port = Port()
        self.run_helper(port, ['02:03:04:05:06:07'], key='A' * 43)
        self.assertEqual(port.frame[:5], b'BKP1\x00')
        self.assertEqual(len(port.frame), 65)

    def test_status_and_replace_are_mutually_exclusive(self):
        with patch.object(sys, 'argv', ['helper', '--port', 'FAKE', '--status', '--replace']), \
                patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=unittest.mock.Mock())}), \
                contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as error:
                helper.main()
            self.assertEqual(error.exception.code, 2)
            sys.modules['serial'].Serial.assert_not_called()

    def test_no_identity_never_writes(self):
        port = Port()
        with patch.object(helper.time, 'monotonic', side_effect=[0, 31]):
            code, text, prompt = self.run_cli(port, ['--status'])
        self.assertEqual(code, 1)
        self.assertIn('no-response', text)
        self.assertEqual(port.frame, b'')
        prompt.assert_not_called()

    def test_open_failure_is_sanitized(self):
        errors = io.StringIO()
        with patch.object(sys, 'argv', ['helper', '--port', 'FAKE']), \
                patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=unittest.mock.Mock(side_effect=OSError('private-fixture')))}), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(errors):
            self.assertEqual(helper.cli(), 1)
        self.assertEqual(errors.getvalue(), 'Diagnostic: serial-error\n')

    def run_helper(self, port, answers, key=' é+/= ', replace=False):
        output = io.StringIO()
        with patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=lambda *a, **k: port)}), \
                patch.object(sys, 'argv', ['helper', '--port', 'FAKE'] + (['--replace'] if replace else [])), \
                patch('builtins.input', side_effect=answers), \
                patch.object(helper.getpass, 'getpass', return_value=key), contextlib.redirect_stdout(output):
            helper.main()
        self.assertNotIn(key, output.getvalue())
        return output.getvalue()

    def test_bytes_and_replace(self):
        p = Port()
        self.run_helper(p, ['02:03:04:05:06:07', 'REPLACE'], replace=True)
        self.assertEqual(p.frame[:5], b'BKP1\x01')
        record = p.frame[7:]
        self.assertEqual(int.from_bytes(p.frame[5:7], 'little'), len(record))
        self.assertEqual(record[11:-4], ' é+/= '.encode())
        self.assertEqual(int.from_bytes(record[-4:], 'little'), zlib.crc32(record[:-4]))

    def test_identity_mismatch_never_writes(self):
        p = Port()
        with self.assertRaises(ValueError):
            self.run_helper(p, ['WRONG'])
        self.assertEqual(p.frame, b'')

    def test_replacement_cancelled(self):
        p = Port()
        with self.assertRaises(ValueError):
            self.run_helper(p, ['02:03:04:05:06:07', 'no'], replace=True)
        self.assertEqual(p.frame, b'')

    def test_rejection_is_not_success(self):
        with self.assertRaises(ValueError):
            self.run_helper(Port(b'ERROR exists\n'), ['02:03:04:05:06:07'])

    def test_oversize(self):
        p = Port()
        with self.assertRaises(ValueError):
            self.run_helper(p, ['02:03:04:05:06:07'], key='é'*65)
        self.assertEqual(p.frame, b'')


if __name__ == '__main__':
    unittest.main()
