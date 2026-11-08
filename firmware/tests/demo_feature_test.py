import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

class DemoFeatureTest(unittest.TestCase):
    def check_flags(self, flags, accepted):
        run = subprocess.run(["g++", "-E", "-P", "-x", "c++", "-Ifirmware/esp32"] + flags + ["-"],
                             input='#include "autonomous_feature.h"\n', text=True,
                             cwd=ROOT, capture_output=True)
        self.assertEqual(run.returncode == 0, accepted, run.stderr)

    def test_default_and_supported(self):
        for flags in ([], ["-DWATERING_AUTONOMOUS_TEST=1"],
                      ["-DWATERING_AUTONOMOUS_TEST=1", "-DWATERING_DEMO_RETRY=1"]):
            with self.subTest(flags=flags): self.check_flags(flags, True)

    def test_reject_legacy_demo(self):
        self.check_flags(["-DWATERING_DEMO_RETRY=1"], False)

    def test_reject_invalid_demo(self):
        self.check_flags(["-DWATERING_AUTONOMOUS_TEST=1", "-DWATERING_DEMO_RETRY=2"], False)
