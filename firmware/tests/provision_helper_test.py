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
