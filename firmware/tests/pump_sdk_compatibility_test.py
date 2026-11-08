"""Host syntax checks of explicit SDK guards, never a target compilation."""
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class PumpSdkCompatibility(unittest.TestCase):
    def compile(self, flag, defines):
        return subprocess.run(["g++", "-std=c++11", "-fsyntax-only", "-DARDUINO",
            "-DWATERING_AUTONOMOUS_TEST=" + str(flag), "-Ifirmware/tests/pump_sdk"] +
            ["-D" + item for item in defines] + ["firmware/esp32/pump.cpp"],
            cwd=ROOT, capture_output=True, text=True)

    def rejected(self, define, message):
        result = self.compile(1, [define])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(message, result.stderr)

    def test_unknown_arduino_core(self):
        self.rejected("ESP_ARDUINO_VERSION=0", "requires audited Arduino ESP32")

    def test_unknown_idf(self):
        self.rejected("ESP_IDF_VERSION=0", "requires audited Arduino ESP32")

    def test_other_target(self):
        self.rejected("CONFIG_IDF_TARGET_ESP32=0", "requires audited ESP32 ISR")

    def test_driver_isr_not_in_iram(self):
        self.rejected("CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=0", "requires audited ESP32 ISR")

    def test_driver_objects_not_internal(self):
        self.rejected("CONFIG_GPTIMER_OBJ_CACHE_SAFE=0", "requires audited ESP32 ISR")

    def test_sdk_clock_not_in_iram(self):
        self.rejected("CONFIG_ESP_TIMER_IN_IRAM=0", "requires audited ESP32 ISR")

    def test_flag_zero_does_not_require_private_sdk(self):
        result = self.compile(0, ["ESP_IDF_VERSION=0", "ESP_ARDUINO_VERSION=0",
                                  "CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM=0", "CONFIG_GPTIMER_OBJ_CACHE_SAFE=0"])
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
