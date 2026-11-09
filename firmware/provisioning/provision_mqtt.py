"""Trusted USB MQTT provisioning. Never pass a password as a command-line argument."""
import argparse
import getpass
from pathlib import Path
import re
import sys
import time
import warnings
import zlib


def wipe(value):
    value[:] = b'\0' * len(value)


class Arguments(argparse.ArgumentParser):
    def error(self, message):
        self.exit(2, 'Invalid command line; use --port, --ca and optional --replace. No password arguments.\n')


def record(identity, host, port, ca, username, password):
    fields = (host, ca, username, password)
    if not re.fullmatch(rb'[A-Za-z0-9.-]{1,253}', host) or not 1 <= port <= 65535:
        raise ValueError('Invalid broker address.')
    for field, maximum in zip(fields, (253, 4095, 63, 127)):
        if not 1 <= len(field) <= maximum or b'\0' in field:
            raise ValueError('Invalid field bounds.')
    if b'-----BEGIN CERTIFICATE-----' not in ca or b'-----END CERTIFICATE-----' not in ca:
        raise ValueError('Invalid PEM envelope; board still must parse certificate.')
    result = bytearray(b'MQ\x01')
    result.extend(bytes.fromhex(identity.replace(':', '')))
    result.extend(port.to_bytes(2, 'little'))
    for field in fields:
        result.extend(len(field).to_bytes(2, 'little'))
    for field in fields:
        result.extend(field)
    result.extend(zlib.crc32(result).to_bytes(4, 'little'))
    return result


def main():
    parser = Arguments(description=__doc__)
    parser.add_argument('--port', required=True, help='USB serial port of the provisioning sketch')
    parser.add_argument('--ca', required=True, type=Path, help='Explicit PEM CA file, at most 4095 bytes')
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    try:
        import serial
    except ImportError:
        sys.exit('Missing pyserial; no dependency was installed.')
    ca, username, password, payload, frame = (bytearray() for _ in range(5))
    try:
        # Read only the explicitly selected file, bounded, without newline conversion.
        with args.ca.open('rb') as source:
            ca.extend(source.read(4096))
        if not 1 <= len(ca) <= 4095:
            raise ValueError('CA file outside bounds.')
        print('Disconnect pump load power. Use the separate MQTT provisioning sketch.')
        with serial.Serial(args.port, 115200, timeout=1, write_timeout=3) as port:
            print('Reset board if its public identity is not displayed.')
            deadline = time.monotonic() + 30
            identity = None
            while time.monotonic() < deadline:
                line = port.read_until(b'\n', 96)
                match = re.fullmatch(rb'MQP1 DEVICE ([0-9A-F]{2}(?::[0-9A-F]{2}){5})\r?\n', line)
                if match:
                    identity = match[1].decode('ascii')
                    break
            if identity is None:
                raise ValueError('No MQTT provisioning identity.')
            print('Device:', identity)
            if input('Confirm the STA MAC from your device/backend: ').upper() != identity:
                raise ValueError('Identity mismatch.')
            if args.replace and input('Type REPLACE followed by a space and that STA MAC: ') != 'REPLACE ' + identity:
                raise ValueError('Replacement cancelled.')
            host = input('Broker hostname (no URI/path): ').encode('ascii')
            port_text = input('Broker TLS port: ')
            if not re.fullmatch(r'[0-9]{1,5}', port_text):
                raise ValueError('Invalid TLS port.')
            with warnings.catch_warnings():
                warnings.simplefilter('error', getpass.GetPassWarning)
                username.extend(getpass.getpass('MQTT username (hidden, exact UTF-8): ').encode('utf-8'))
                password.extend(getpass.getpass('MQTT password (hidden, exact UTF-8; not BLE key): ').encode('utf-8'))
            payload = record(identity, host, int(port_text), ca, username, password)
            frame.extend(b'MQP1')
            frame.append(int(args.replace))
            frame.extend(len(payload).to_bytes(2, 'little'))
            frame.extend(payload)
            if port.write(frame) != len(frame):
                raise ValueError('Incomplete write; unconfirmed.')
            port.flush()
            response = port.read_until(b'\n', 96)
            if response not in (b'OK verified\n', b'OK verified\r\n'):
                statuses = {
                    b'ERROR exists': 'Existing config refused; replacement requires explicit authorization.',
                    b'ERROR invalid': 'Invalid config; no verified write.',
                    b'ERROR not-written': 'Storage unavailable before writing; config not provisioned.',
                    b'ERROR unconfirmed': 'Write unconfirmed; old config is not guaranteed. Inspect and provision again after reset.',
                }
                print(statuses.get(response.rstrip(b'\r\n'), 'Provisioning rejected or unconfirmed.'))
                raise ValueError('Rejected/unconfirmed; no automatic retry.')
            print('Verified. Reflash production with the same Min SPIFFS layout and full erase DISABLED.')
    finally:
        for value in (ca, username, password, payload, frame):
            wipe(value)


if __name__ == '__main__':
    try:
        main()
    except (Exception, KeyboardInterrupt):
        # Serial/encoding exception messages can contain input; never print them.
        sys.exit('MQTT provisioning failed or cancelled; success is not confirmed. No automatic retry.')
