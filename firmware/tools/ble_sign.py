"""Sign BLE JSON locally for manual use in nRF Connect; never sends commands."""

import argparse
from decimal import Decimal, InvalidOperation
import getpass
import hmac
import hashlib
import json
import re
import time


def sign_command(key, nonce, cmd, dur):
    """Contract signer; interactive safety limits are applied separately."""
    if re.fullmatch(r"[0-9a-f]{16}", nonce) is None:
        raise ValueError("Nonce must be exactly 16 lowercase hex characters.")
    if cmd not in ("PUMP_OFF", "PUMP_ON"):
        raise ValueError("Invalid pump command.")
    if type(dur) is not int or dur < 0 or (cmd == "PUMP_OFF" and dur != 0):
        raise ValueError("Invalid duration.")
    canonical = f"{nonce}|{cmd}|{dur}"
    sig = hmac.new(key.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()[:16]
    return json.dumps({"cmd": cmd, "dur": dur, "sig": sig}, separators=(",", ":"))


def read_hidden(prompt):
    # getpass invokes this fallback when it cannot disable terminal echo.
    # Replace it before prompting so no visible key input can occur.
    def refuse_echo(*args, **kwargs):
        raise RuntimeError("Cannot disable input echo; aborted before reading secret.")

    previous = getpass.fallback_getpass
    getpass.fallback_getpass = refuse_echo
    try:
        return getpass.getpass(prompt)
    finally:
        getpass.fallback_getpass = previous


def read_key():
    return read_hidden("ble_secret_key (hidden, exact UTF-8): ")


def sign_time(key, nonce, unix):
    """Sign an explicit Unix timestamp using the TIME contract."""
    if re.fullmatch(r"[0-9a-f]{16}", nonce) is None:
        raise ValueError("Nonce must be exactly 16 lowercase hex characters.")
    if type(unix) is not int or unix < 0:
        raise ValueError("Invalid Unix seconds.")
    canonical = f"{nonce}|TIME|{unix}"
    sig = hmac.new(key.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()[:16]
    return json.dumps({"unix": unix, "sig": sig}, separators=(",", ":"))


def validate_wifi(ssid, password):
    for value, minimum, maximum in ((ssid, 1, 32), (password, 8, 63)):
        if "|" in value or "\0" in value:
            raise ValueError("SSID/password must not contain pipe or NUL.")
        if not minimum <= len(value.encode("utf-8")) <= maximum:
            raise ValueError("SSID requires 1-32 UTF-8 bytes; password requires 8-63.")


def sign_wifi(key, nonce, ssid, password, mtu=185):
    validate_wifi(ssid, password)
    if re.fullmatch(r"[0-9a-f]{16}", nonce) is None:
        raise ValueError("Nonce must be exactly 16 lowercase hex characters.")
    if type(mtu) is not int or not 23 <= mtu <= 517:
        raise ValueError("Actual ATT MTU must be an integer from 23 to 517.")
    canonical = f"{nonce}|WIFI|{ssid}|{password}"
    sig = hmac.new(key.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()[:16]
    payload = json.dumps({"ssid": ssid, "pass": password, "sig": sig}, ensure_ascii=False, separators=(",", ":"))
    if len(payload.encode("utf-8")) > mtu - 3:
        raise ValueError("Serialized WIFI JSON exceeds MTU-3; no payload emitted. No splitting or truncation.")
    return payload


def sign_config(key, nonce, min_percent="30.0", max_percent="70.0", auto=0,
                max_dur=120, min_water="25.0", mtu=185):
    """Sign operational calibrated percent, retaining exact decimal tenths."""
    if re.fullmatch(r"[0-9a-f]{16}", nonce) is None:
        raise ValueError("Nonce must be exactly 16 lowercase hex characters.")

    def tenths(value):
        text = str(value)
        if re.fullmatch(r"(?:0|[1-9][0-9]*)(?:\.[0-9])?", text) is None:
            raise ValueError("CONFIG percentages require integers or one decimal digit.")
        try:
            number = Decimal(text)
            if not 0 <= number <= 100:
                raise ValueError("CONFIG percentage outside 0..100.")
            return number
        except InvalidOperation:
            raise ValueError("Invalid CONFIG percentage.") from None

    minimum, maximum, water = map(tenths, (min_percent, max_percent, min_water))
    # Permit below-policy thresholds for signed rejection tests. Firmware owns
    # business validation; never clamp or substitute the value being signed.
    if not minimum < maximum:
        raise ValueError("Require min < max.")
    if type(auto) is not int or auto not in (0, 1):
        raise ValueError("CONFIG auto must be integer 0 or 1.")
    if type(max_dur) is not int or not 60 <= max_dur <= 600:
        raise ValueError("CONFIG max_dur must be integer seconds in 60..600.")
    if type(mtu) is not int or not 23 <= mtu <= 517:
        raise ValueError("Actual ATT MTU must be an integer from 23 to 517.")
    canonical = f"{nonce}|CONFIG|{minimum:.1f}|{maximum:.1f}|{auto}|{max_dur}|{water:.1f}"
    sig = hmac.new(key.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()[:16]
    payload = (f'{{"min":{minimum:.1f},"max":{maximum:.1f},"auto":{auto},'
               f'"max_dur":{max_dur},"min_water":{water:.1f},"sig":"{sig}"}}')
    if len(payload.encode("utf-8")) > min(192, mtu - 3):
        raise ValueError("CONFIG JSON exceeds min(192, MTU-3); no splitting or truncation.")
    return payload


def main(config_only=False, mtu=185, min_water="25.0", auto=0):
    try:
        key = read_key()
        print("Signed JSON is single-use. Nonce expires after 30 seconds from issuance.")
        print("Payload contains no nonce field. Copy JSON manually into nRF Connect.")
        while True:
            cmd = "CONFIG" if config_only else input("Command [PUMP_OFF] / PUMP_ON / TIME / WIFI: ") or "PUMP_OFF"
            if cmd not in ("PUMP_OFF", "PUMP_ON", "TIME", "WIFI", "CONFIG"):
                print("Choose PUMP_OFF, PUMP_ON, TIME or WIFI.")
                continue
            dur = 0
            if cmd == "PUMP_ON":
                duration = input("Duration in seconds (1-5): ")
                if re.fullmatch(r"[1-5]", duration) is None:
                    print("Interactive PUMP_ON requires 1-5 seconds.")
                    continue
                dur = int(duration)
            if cmd == "WIFI":
                print("WARNING: output JSON contains the Wi-Fi password. Do not log/save it or put it in chat or reports.")
                ssid = input("SSID (exact UTF-8): ")
                password = read_hidden("Wi-Fi password (hidden, exact UTF-8): ")
                try:
                    validate_wifi(ssid, password)
                    mtu_text = input("Actual negotiated ATT MTU [185]: ") or "185"
                    if re.fullmatch(r"[0-9]{2,3}", mtu_text) is None or not 23 <= int(mtu_text) <= 517:
                        raise ValueError("Actual ATT MTU must be an integer from 23 to 517.")
                    mtu = int(mtu_text)
                except ValueError as error:
                    print(str(error))
                    del ssid, password
                    continue
            if cmd == "CONFIG":
                print(f"CONFIG: min=30.0 max=70.0 auto={auto} max_dur=120s min_water={min_water} operational %.")
                print("Subscribe 0005, read fresh nonce0004 now; write JSON to 0003 With Response within 30s of nonce issuance.")
            # Read nonce last. Do not retain it between signing iterations.
            nonce = input("Fresh nonce (16 lowercase hex): ")
            try:
                if cmd == "CONFIG":
                    payload = sign_config(key, nonce, auto=auto, min_water=min_water, mtu=mtu)
                elif cmd == "TIME":
                    payload = sign_time(key, nonce, int(time.time()))
                elif cmd == "WIFI":
                    payload = sign_wifi(key, nonce, ssid, password, mtu)
                else:
                    payload = sign_command(key, nonce, cmd, dur)
            except ValueError as error:
                print(str(error))
                continue
            finally:
                del nonce
                if cmd == "WIFI":
                    del ssid, password
            print(payload)
            del payload
            if input("Sign another command with a fresh nonce? [y/N]: ") != "y":
                break
    except (EOFError, KeyboardInterrupt):
        print("\nAborted.")
    except RuntimeError as error:
        print(str(error))
        return 1
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", action="store_true", help="Sign CONFIG preset 30.0/70.0/auto0/120s/water25.0%%; prompt for hidden key then nonce.")
    parser.add_argument("--mtu", type=int, default=185, help="Actual negotiated ATT MTU for CONFIG (default185).")
    parser.add_argument("--min-water", "--min_water", default="25.0", help="CONFIG operational percent (default25.0); allows 19.9 for firmware rejection testing. Integer or one decimal digit, 0..100.")
    parser.add_argument("--auto", type=int, choices=(0, 1), default=0, help="CONFIG automatic mode: 0 or 1 (default0).")
    args = parser.parse_args()
    if not 23 <= args.mtu <= 517:
        parser.error("MTU must be in 23..517.")
    raise SystemExit(main(config_only=args.config, mtu=args.mtu, min_water=args.min_water, auto=args.auto))
