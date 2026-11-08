"""Offline tests using fixture keys only; no device access."""

import contextlib
import hashlib
import hmac
import io
import json
import unittest
from unittest.mock import patch

from firmware.tools import ble_sign


KEY = "test-ble-secret-key"
NONCE = "84a9f21b3c0e5a1d"


class BleSignTest(unittest.TestCase):
    def test_config_signer_contract_and_bounds(self):
        vector = ble_sign.sign_config(KEY, NONCE, auto=1, min_water="20.0")
        self.assertEqual(json.loads(vector)["sig"], "a4426d27c5dddb70")
        payload = ble_sign.sign_config(KEY, NONCE)
        canonical = f"{NONCE}|CONFIG|30.0|70.0|0|120|25.0"
        expected = hmac.new(KEY.encode(), canonical.encode(), hashlib.sha256).hexdigest()[:16]
        self.assertEqual(payload, '{"min":30.0,"max":70.0,"auto":0,"max_dur":120,"min_water":25.0,"sig":"' + expected + '"}')
        size = len(payload.encode())
        self.assertEqual(ble_sign.sign_config(KEY, NONCE, mtu=size + 3), payload)
        with self.assertRaises(ValueError):
            ble_sign.sign_config(KEY, NONCE, mtu=size + 2)
        for water in ("5.0", "19.9", "20.0", "25.1", "100.0"):
            ble_sign.sign_config(KEY, NONCE, min_water=water)
        for water in ("100.1", "25.00", "NaN", "2e1"):
            with self.assertRaises(ValueError):
                ble_sign.sign_config(KEY, NONCE, min_water=water)
        for overrides in ({"auto":True}, {"max_dur":120.0}, {"max_dur":59}, {"min_percent":"70.0"}):
            with self.assertRaises(ValueError):
                ble_sign.sign_config(KEY, NONCE, **overrides)

    def test_config_preset_prompts_hidden_key_before_nonce(self):
        events = []
        def hidden():
            events.append("key")
            return KEY
        def prompt(message):
            if "Fresh nonce" in message:
                self.assertEqual(events, ["key"])
                events.append("nonce")
                return NONCE
            return "n"
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", side_effect=hidden), patch("builtins.input", side_effect=prompt), contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(config_only=True), 0)
        self.assertEqual(events, ["key", "nonce"])
        self.assertNotIn(KEY, output.getvalue())
        self.assertNotIn(NONCE, output.getvalue())
        self.assertIn(ble_sign.sign_config(KEY, NONCE), output.getvalue())

    def test_config_percent_vector_independent(self):
        canonical = "84a9f21b3c0e5a1d|CONFIG|30.0|70.0|1|120|20.0"
        self.assertEqual(hmac.new(KEY.encode("utf-8"), canonical.encode("utf-8"), hashlib.sha256).hexdigest()[:16], "a4426d27c5dddb70")

    def test_config_out_of_business_range_signed_as_sent(self):
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch("builtins.input", side_effect=[NONCE, "n"]), contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(config_only=True, min_water="19.9"), 0)
        payload = next(line for line in output.getvalue().splitlines() if line.startswith("{"))
        data = json.loads(payload)
        self.assertEqual(data["min_water"], 19.9)
        canonical = f"{NONCE}|CONFIG|30.0|70.0|0|120|19.9"
        expected = hmac.new(KEY.encode(), canonical.encode(), hashlib.sha256).hexdigest()[:16]
        self.assertEqual(data["sig"], expected)
        self.assertNotIn(KEY, output.getvalue())

    def test_config_auto_enabled_signed_as_sent(self):
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch("builtins.input", side_effect=[NONCE, "n"]), contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(config_only=True, auto=1), 0)
        payload = next(line for line in output.getvalue().splitlines() if line.startswith("{"))
        data = json.loads(payload)
        self.assertEqual(data["auto"], 1)
        canonical = f"{NONCE}|CONFIG|30.0|70.0|1|120|25.0"
        self.assertEqual(data["sig"], hmac.new(KEY.encode(), canonical.encode(), hashlib.sha256).hexdigest()[:16])
        self.assertNotIn(KEY, output.getvalue())

    def test_wifi_contract_vector(self):
        payload = ble_sign.sign_wifi(KEY, NONCE, "HomeWiFi", "12345678")
        self.assertEqual(json.loads(payload)["sig"], "ca81265e275b9a4a")
        self.assertEqual(set(json.loads(payload)), {"ssid", "pass", "sig"})
        self.assertEqual(payload, json.dumps(json.loads(payload), separators=(",", ":")))

    def test_wifi_utf8_preserved_and_mtu_boundary(self):
        ssid, password = " e\u0301 ", " \u00e9\"\\12345 "
        payload = ble_sign.sign_wifi(KEY, NONCE, ssid, password)
        data = json.loads(payload)
        self.assertEqual((data["ssid"], data["pass"]), (ssid, password))
        expected = hmac.new(KEY.encode(), f"{NONCE}|WIFI|{ssid}|{password}".encode(), hashlib.sha256).hexdigest()[:16]
        self.assertEqual(data["sig"], expected)
        size = len(payload.encode("utf-8"))
        self.assertEqual(ble_sign.sign_wifi(KEY, NONCE, ssid, password, size + 3), payload)
        with self.assertRaises(ValueError):
            ble_sign.sign_wifi(KEY, NONCE, ssid, password, size + 2)

    def test_wifi_field_bounds_and_rejections(self):
        for ssid, password in (("a", "a" * 8), ("\u00e9" * 16, "a" * 63)):
            ble_sign.sign_wifi(KEY, NONCE, ssid, password)
        for ssid, password in (("", "12345678"), ("\u00e9" * 17, "12345678"), ("a", "a" * 7), ("a", "\u00e9" * 32), ("a|b", "12345678"), ("a\0b", "12345678"), ("a", "1234567|"), ("a", "1234567\0")):
            with self.assertRaises(ValueError):
                ble_sign.sign_wifi(KEY, NONCE, ssid, password)
        for nonce in (NONCE.upper(), NONCE + "\n"):
            with self.assertRaises(ValueError):
                ble_sign.sign_wifi(KEY, nonce, "HomeWiFi", "12345678")

    def test_interactive_wifi_order_and_default_mtu(self):
        events = []
        answers = iter(["WIFI", "HomeWiFi", "", NONCE, "n"])
        def prompt(message):
            answer = next(answers)
            events.append(answer)
            return answer
        def hidden(message):
            self.assertEqual(events, ["WIFI", "HomeWiFi"])
            events.append("hidden")
            return "12345678"
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch.object(ble_sign, "read_hidden", side_effect=hidden), patch("builtins.input", side_effect=prompt), contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(), 0)
        self.assertEqual(events, ["WIFI", "HomeWiFi", "hidden", "", NONCE, "n"])
        payloads = [line for line in output.getvalue().splitlines() if line.startswith("{")]
        self.assertEqual(payloads, [ble_sign.sign_wifi(KEY, NONCE, "HomeWiFi", "12345678")])
        self.assertIn("WARNING", output.getvalue())

    def test_interactive_wifi_oversize_emits_no_json(self):
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch.object(ble_sign, "read_hidden", return_value="12345678"), patch("builtins.input", side_effect=["WIFI", "HomeWiFi", "23", NONCE, EOFError]), contextlib.redirect_stdout(output):
            ble_sign.main()
        self.assertFalse(any(line.startswith("{") for line in output.getvalue().splitlines()))
        self.assertNotIn("12345678", output.getvalue())

    def test_time_contract_vector(self):
        self.assertEqual(
            ble_sign.sign_time(KEY, NONCE, 1793800000),
            '{"unix":1793800000,"sig":"26b87998f4bc4778"}',
        )

    def test_interactive_time_reads_clock_after_nonce(self):
        events = []
        answers = iter(["TIME", NONCE, "n"])

        def prompt(message):
            answer = next(answers)
            events.append(answer)
            return answer

        def clock():
            self.assertEqual(events, ["TIME", NONCE])
            events.append("clock")
            return 1793800000.9

        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch("builtins.input", side_effect=prompt), patch.object(ble_sign.time, "time", side_effect=clock) as now, contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(), 0)
        now.assert_called_once_with()
        payloads = [line for line in output.getvalue().splitlines() if line.startswith("{")]
        self.assertEqual(payloads, ['{"unix":1793800000,"sig":"26b87998f4bc4778"}'])
        self.assertNotIn(KEY, output.getvalue())
        self.assertNotIn(NONCE, output.getvalue())

    def test_contract_vectors(self):
        for cmd, dur, sig in (
            ("PUMP_ON", 300, "48ba618920e3dcb4"),
            ("PUMP_OFF", 0, "238041d3721ef169"),
        ):
            self.assertEqual(
                ble_sign.sign_command(KEY, NONCE, cmd, dur),
                json.dumps({"cmd": cmd, "dur": dur, "sig": sig}, separators=(",", ":")),
            )

    def test_exact_utf8_key(self):
        key = "  khóa+/=\t "
        expected = hmac.new(key.encode("utf-8"), (NONCE + "|PUMP_OFF|0").encode("utf-8"), hashlib.sha256).hexdigest()[:16]
        self.assertEqual(json.loads(ble_sign.sign_command(key, NONCE, "PUMP_OFF", 0))["sig"], expected)

    def test_nonce_validation(self):
        for nonce in (NONCE.upper(), NONCE + "\n", " " + NONCE, NONCE[:-1], "g" * 16):
            with self.assertRaises(ValueError):
                ble_sign.sign_command(KEY, nonce, "PUMP_OFF", 0)
            with self.assertRaises(ValueError):
                ble_sign.sign_time(KEY, nonce, 1793800000)

    def test_getpass_echo_fallback_aborts(self):
        original = ble_sign.getpass.fallback_getpass
        def fallback(*args, **kwargs):
            return ble_sign.getpass.fallback_getpass(*args, **kwargs)
        with patch.object(ble_sign.getpass, "getpass", side_effect=fallback), patch("builtins.input") as prompt:
            with self.assertRaises(RuntimeError):
                ble_sign.read_key()
            prompt.assert_not_called()
        self.assertIs(ble_sign.getpass.fallback_getpass, original)

    def test_interactive_limits_and_fresh_nonce(self):
        inputs = ["PUMP_ON", "300", "", NONCE, "y", "PUMP_ON", "5", "0123456789abcdef", "n"]
        output = io.StringIO()
        with patch.object(ble_sign, "read_key", return_value=KEY), patch("builtins.input", side_effect=inputs), contextlib.redirect_stdout(output):
            self.assertEqual(ble_sign.main(), 0)
        payloads = [json.loads(line) for line in output.getvalue().splitlines() if line.startswith("{")]
        self.assertEqual(len(payloads), 2)
        self.assertEqual(payloads[0], json.loads(ble_sign.sign_command(KEY, NONCE, "PUMP_OFF", 0)))
        self.assertEqual(payloads[1], json.loads(ble_sign.sign_command(KEY, "0123456789abcdef", "PUMP_ON", 5)))
        for payload in payloads:
            self.assertEqual(set(payload), {"cmd", "dur", "sig"})
        self.assertNotIn(KEY, output.getvalue())
        self.assertNotIn(NONCE, output.getvalue())


if __name__ == "__main__":
    unittest.main()
