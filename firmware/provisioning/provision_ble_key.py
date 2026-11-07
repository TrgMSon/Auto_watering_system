"""Trusted USB provisioning; never accepts a secret argument or prints a secret."""
import argparse
import getpass
import re
import sys
import time
import warnings
import zlib


def wipe(value):
    value[:] = b'\0' * len(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        sys.exit('Missing pyserial; no dependency was installed.')
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
                raise ValueError('No provisioning identity; reset board and retry.')
            print('Device:', identity)
            if input('Confirm by typing the STA MAC from your device/backend: ').upper() != identity:
                raise ValueError('Identity confirmation failed.')
            if args.replace and input('Type REPLACE to authorize replacement: ') != 'REPLACE':
                raise ValueError('Replacement cancelled.')
            # Fail if getpass cannot disable echo; never fall back to visible input.
            with warnings.catch_warnings():
                warnings.simplefilter('error', getpass.GetPassWarning)
                key = bytearray(getpass.getpass('ble_secret_key (hidden, exact UTF-8): ').encode('utf-8'))
            if not 1 <= len(key) <= 128:
                raise ValueError('Key must be 1..128 UTF-8 bytes.')
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
                raise ValueError('Incomplete write; success unconfirmed.')
            port.flush()
            response = port.read_until(b'\n', 96).strip()
            if response != b'OK verified':
                # Do not print arbitrary input from the port.
                raise ValueError('Provisioning rejected or unconfirmed; inspect status and retry after reset.')
            print('Verified. Reflash production with full flash erase DISABLED.')
    finally:
        wipe(key)
        wipe(record)
        wipe(frame)


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt):
        # Exception messages from serial/encoding must not expose input buffers.
        sys.exit('Provisioning failed or cancelled; success is not confirmed.')
