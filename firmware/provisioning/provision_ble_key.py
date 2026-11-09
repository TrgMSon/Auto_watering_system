"""Trusted USB provisioning; never accepts a secret argument or prints a secret."""
import argparse
import getpass
import re
import sys
import time
import warnings
import zlib


class DiagnosticError(ValueError):
    """Only fixed, non-sensitive categories may cross the CLI boundary."""


RESPONSES = {
    b'ERROR exists': 'existing-record',
    b'ERROR invalid': 'invalid-record',
    b'ERROR storage': 'storage-or-readback-failure',
    b'ERROR frame': 'frame-error',
    b'ERROR timeout': 'frame-timeout',
}
STATUSES = {b'STATUS missing', b'STATUS valid', b'STATUS invalid', b'STATUS unreadable'}


def check_response(response, status=False):
    if status and response in STATUSES:
        return response.decode('ascii')
    if not status and response == b'OK verified':
        return None
    raise DiagnosticError(RESPONSES.get(response, 'no-response' if not response else 'unexpected-response'))


def wipe(value):
    value[:] = b'\0' * len(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--replace', action='store_true')
    mode.add_argument('--status', action='store_true', help='Read BLE record status only; never asks for a key or writes NVS.')
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        raise DiagnosticError('serial-error') from None
    print('Disconnect pump load power. Upload provisioning sketch first.')
    key = bytearray()
    record = bytearray()
    frame = bytearray()
    try:
        with serial.Serial(args.port, 115200, timeout=1, write_timeout=3) as port:
            print('Reset board now if identity is not displayed.')
            deadline = time.monotonic() + 30
            identity = None
            while time.monotonic() < deadline:
                line = port.read_until(b'\n', 96)
                match = re.fullmatch(rb'BKP1 DEVICE ([0-9A-F]{2}(?::[0-9A-F]{2}){5})\r?\n', line)
                if match:
                    identity = match[1].decode('ascii')
                    break
            if identity is None:
                raise DiagnosticError('no-response')
            print('Device:', identity)
            if args.status:
                if port.write(b'BKS1') != 4:
                    raise DiagnosticError('serial-error')
                port.flush()
                print(check_response(port.read_until(b'\n', 96).strip(), status=True))
                return
            if input('Confirm by typing the STA MAC from your device/backend: ').upper() != identity:
                raise DiagnosticError('invalid-record')
            if args.replace and input('Type REPLACE to authorize replacement: ') != 'REPLACE':
                raise DiagnosticError('invalid-record')
            # Fail if getpass cannot disable echo; never fall back to visible input.
            try:
                with warnings.catch_warnings():
                    warnings.simplefilter('error', getpass.GetPassWarning)
                    key = bytearray(getpass.getpass('ble_secret_key (hidden, exact UTF-8): ').encode('utf-8'))
            except (Exception, KeyboardInterrupt):
                raise DiagnosticError('hidden-input-error') from None
            if not 1 <= len(key) <= 128:
                raise DiagnosticError('invalid-record')
            record.extend(b'BK\x01')
            record.extend(bytes.fromhex(identity.replace(':', '')))
            record.extend(len(key).to_bytes(2, 'little'))
            record.extend(key)
            record.extend(zlib.crc32(record).to_bytes(4, 'little'))
            frame.extend(b'BKP1')
            frame.append(int(args.replace))
            frame.extend(len(record).to_bytes(2, 'little'))
            frame.extend(record)
            if port.write(frame) != len(frame):
                raise DiagnosticError('serial-error')
            port.flush()
            response = port.read_until(b'\n', 96).strip()
            check_response(response)
            print('Verified. Reflash production with full flash erase DISABLED.')
    except DiagnosticError:
        raise
    except (EOFError, KeyboardInterrupt):
        raise DiagnosticError('hidden-input-error') from None
    except Exception:
        raise DiagnosticError('serial-error') from None
    finally:
        wipe(key)
        wipe(record)
        wipe(frame)


def cli():
    try:
        main()
        return 0
    except DiagnosticError as error:
        # Never interpolate an exception message, even for our own exception type.
        category = error.args[0] if error.args else None
        allowed = set(RESPONSES.values()) | {'no-response', 'unexpected-response', 'hidden-input-error', 'serial-error'}
        print('Diagnostic: ' + (category if category in allowed else 'unexpected-response'), file=sys.stderr)
        return 1
    except (Exception, KeyboardInterrupt):
        print('Diagnostic: unexpected-response', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(cli())
