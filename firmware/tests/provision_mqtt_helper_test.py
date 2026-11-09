"""USB helper tests: temporary fixture PEM + Serial mock, never real Serial/NVS."""
import contextlib
import importlib.util
import io
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch
import warnings
import zlib

spec = importlib.util.spec_from_file_location('mqtt_helper', Path(__file__).parents[1] / 'provisioning/provision_mqtt.py')
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)
MAC = '02:03:04:05:06:07'
PEM = b'-----BEGIN CERTIFICATE-----\r\nfixture\r\n-----END CERTIFICATE-----\r\n'


class Port:
    def __init__(self, response=b'OK verified\n', greeting=None, short=False):
        self.lines = [greeting or ('MQP1 DEVICE ' + MAC + '\n').encode(), response]
        self.frame = b''
        self.view = None
        self.short = short

    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def read_until(self, *args):
        return self.lines.pop(0) if self.lines else b''

    def write(self, data):
        self.frame = bytes(data)
        self.view = memoryview(data)
        return len(data) - int(self.short)

    def flush(self):
        pass


class Tests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.ca = Path(self.directory.name) / 'operator-selected-fixture.pem'
        self.ca.write_bytes(PEM)

    def run_helper(self, port, answers=None, replace=False, hidden=None, ticks=None):
        output = io.StringIO()
        answers = answers if answers is not None else [MAC, 'broker.example.invalid', '8883']
        hidden = hidden if hidden is not None else ['fixture-mqtt-user', ' é+/= ']
        args = ['helper', '--port', 'MOCK', '--ca', str(self.ca)] + (['--replace'] if replace else [])
        with patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=lambda *a, **k: port)}), \
                patch.object(sys, 'argv', args), patch('builtins.input', side_effect=answers), \
                patch.object(helper.getpass, 'getpass', side_effect=hidden), \
                patch.object(helper.time, 'monotonic', side_effect=ticks or (lambda: 0)), \
                contextlib.redirect_stdout(output):
            helper.main()
        for value in [' é+/= ', 'fixture-mqtt-user', 'broker.example.invalid', 'BEGIN CERTIFICATE']:
            self.assertNotIn(value, output.getvalue())
        return output.getvalue()

    def test_exact_roundtrip(self):
        p = Port()
        output = self.run_helper(p)
        self.assertIn('Verified.', output)
        self.assertEqual(p.frame[:5], b'MQP1\x00')
        data = p.frame[7:]
        self.assertEqual(int.from_bytes(p.frame[5:7], 'little'), len(data))
        self.assertEqual(data[:3], b'MQ\x01')
        self.assertEqual(data[3:9], bytes.fromhex(MAC.replace(':', '')))
        self.assertEqual(int.from_bytes(data[9:11], 'little'), 8883)
        lengths = [int.from_bytes(data[11+i*2:13+i*2], 'little') for i in range(4)]
        offset = 19
        fields = []
        for length in lengths:
            fields.append(data[offset:offset+length])
            offset += length
        self.assertEqual(fields, [b'broker.example.invalid', PEM, b'fixture-mqtt-user', ' é+/= '.encode()])
        self.assertEqual(offset, len(data)-4)
        self.assertEqual(int.from_bytes(data[-4:], 'little'), zlib.crc32(data[:-4]))
        self.assertTrue(p.view is not None and all(byte == 0 for byte in p.view))

    def test_replace_requires_confirmation(self):
        p = Port()
        self.run_helper(p, [MAC, 'REPLACE '+MAC, 'broker.example.invalid', '8883'], replace=True)
        self.assertEqual(p.frame[4], 1)
        p = Port()
        with self.assertRaises(ValueError):
            self.run_helper(p, [MAC, 'REPLACE'], replace=True)
        self.assertFalse(p.frame)

    def test_mac_mismatch(self):
        p = Port()
        with self.assertRaises(ValueError):
            self.run_helper(p, ['02:03:04:05:06:08'])
        self.assertFalse(p.frame)

    def test_rejects_bkp1_greeting(self):
        p = Port(greeting=('BKP1 DEVICE '+MAC+'\n').encode())
        with self.assertRaises(ValueError):
            self.run_helper(p, ticks=[0, 0, 31])
        self.assertFalse(p.frame)

    def test_no_false_verified_response(self):
        for response in (b'ERROR exists\n', b'ERROR unconfirmed\n', b'OK verified', b'noise OK verified\n', b''):
            with self.subTest(response=response), self.assertRaises(ValueError):
                self.run_helper(Port(response))

    def test_short_write(self):
        port = Port(short=True)
        with self.assertRaises(ValueError):
            self.run_helper(port)
        self.assertTrue(all(byte == 0 for byte in port.view))

    def test_host_bounds_and_port(self):
        for host in ('ssl://example.invalid', 'user@example.invalid', 'example.invalid/path', 'a'*254):
            p = Port()
            with self.assertRaises(ValueError):
                self.run_helper(p, [MAC, host, '8883'])
            self.assertFalse(p.frame)
        for port in ('0', '65536', ' 8883', 'abc'):
            with self.assertRaises(ValueError):
                self.run_helper(Port(), [MAC, 'broker.example.invalid', port])

    def test_credentials_bounds_and_nul(self):
        for username, password in (('', 'x'), ('u'*64, 'x'), ('u', ''), ('u', 'é'*64), ('u', 'a\0b')):
            p = Port()
            with self.assertRaises(ValueError):
                self.run_helper(p, hidden=[username, password])
            self.assertFalse(p.frame)

    def test_hidden_input_has_no_echo_fallback(self):
        def cannot_hide(*args):
            warnings.warn('fixture terminal cannot hide input', helper.getpass.GetPassWarning)
        with patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=lambda *a, **k: Port())}), \
                patch.object(sys, 'argv', ['helper', '--port', 'MOCK', '--ca', str(self.ca)]), \
                patch('builtins.input', side_effect=[MAC, 'broker.example.invalid', '8883']), \
                patch.object(helper.getpass, 'getpass', side_effect=cannot_hide), contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(helper.getpass.GetPassWarning):
                helper.main()

    def test_oversize_file_never_opens_port(self):
        self.ca.write_bytes(b'x'*4096)
        constructor = unittest.mock.Mock()
        with patch.dict(sys.modules, {'serial': types.SimpleNamespace(Serial=constructor)}), \
                patch.object(sys, 'argv', ['helper', '--port', 'MOCK', '--ca', str(self.ca)]):
            with self.assertRaises(ValueError):
                helper.main()
        constructor.assert_not_called()

    def test_no_password_argument_or_echo_in_usage_error(self):
        output = io.StringIO()
        with patch.object(sys, 'argv', ['helper', '--password', 'fixture-secret-sentinel']), contextlib.redirect_stderr(output):
            with self.assertRaises(SystemExit):
                helper.main()
        self.assertNotIn('fixture-secret-sentinel', output.getvalue())

    def test_missing_serial_dependency(self):
        with patch.dict(sys.modules, {'serial': None}), \
                patch.object(sys, 'argv', ['helper', '--port', 'MOCK', '--ca', str(self.ca)]):
            with self.assertRaisesRegex(SystemExit, 'Missing pyserial'):
                helper.main()


if __name__ == '__main__':
    unittest.main()
