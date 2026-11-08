"""Host-only regression runner. Never builds targets, flashes or accesses hardware."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
P = "firmware/esp32/"
OWNER = [P + x + ".cpp" for x in ("pump", "watering", "irrigation")]
BRIDGE = [P + x + ".cpp" for x in ("mqtt_commands", "ble_dispatch", "ble_command_bridge", "ble_auth", "ble_wifi_parser")]
TLS = [P + "mqtt_tls.cpp", "firmware/tests/mqtt_sdk/tls_fake.cpp"]


def specs(path):
    name = path.stem
    flags, sources, libs = [], [], []
    variants = [("", [])]
    if name == "pump_sdk_test":
        flags, sources = ["-DARDUINO", "-Ifirmware/tests/pump_sdk"], [P + "pump.cpp"]
        variants = [("_flag0", ["-DWATERING_AUTONOMOUS_TEST=0"]),
                    ("_flag1", ["-DWATERING_AUTONOMOUS_TEST=1"])]
    elif path.parent.name == "pump":
        flags, sources = ["-Ifirmware/tests/pump"], OWNER
        if name == "guard_harness_test":
            flags += ["-DF31D_GUARD_TEST=1"]
    elif name == "autonomous_owner_test":
        flags = ["-DWATERING_AUTONOMOUS_TEST=1", "-Ifirmware/tests/pump"]
        sources = OWNER + BRIDGE + [P + "autonomous_storage.cpp", P + "autonomous_owner.cpp"]
    elif name == "autonomous_lease_rejection_test":
        flags = ["-DWATERING_AUTONOMOUS_TEST=1", "-Ifirmware/tests/pump"]
        sources = [P + x + ".cpp" for x in ("watering", "autonomous_storage", "autonomous_owner")]
    elif name == "demo_retry_profile_test":
        flags = ["-DWATERING_AUTONOMOUS_TEST=1"]
        sources = [P + "watering.cpp", P + "autonomous_storage.cpp"]
        variants = [("_production", ["-DWATERING_DEMO_RETRY=0"]),
                    ("_demo", ["-DWATERING_DEMO_RETRY=1"])]
    elif name == "autonomous_storage_test":
        sources = [P + "watering.cpp", P + "autonomous_storage.cpp"]
    elif name == "autonomous_nvs_test":
        flags = ["-Ifirmware/tests/mqtt_nvs"]
    elif name in ("watering_test", "autonomous_watering_test"):
        sources = [P + "watering.cpp"]
    elif name == "sensors_test":
        flags, sources = ["-Ifirmware/tests"], [P + "sensors.cpp"]
    elif name in ("ble_telemetry_test", "mqtt_telemetry_test"):
        flags, sources = ["-Ifirmware/tests/pump"], OWNER
    elif name in ("ble_clock_platform_test", "ble_pairing_test"):
        pass
    elif name == "ble_pairing_sdk_test":
        flags = ["-DARDUINO", "-Ifirmware/tests/pairing_sdk"]
    elif name == "ble_keys_test":
        sources = [P + "ble_auth.cpp"]
    elif name == "wifi_runtime_test":
        sources = [P + "ble_auth.cpp", P + "ble_wifi_parser.cpp"]
    elif name == "wifi_esp32_sdk_test":
        flags = ["-DARDUINO", "-Ifirmware/tests/wifi_sdk"]
        sources = [P + x + ".cpp" for x in ("ble_auth", "ble_wifi_parser", "wifi_esp32_adapters")]
        sources += ["firmware/tests/wifi_sdk/sdk_fake.cpp"]
    elif name == "mqtt_provision_test":
        flags = ["-Ifirmware/tests/mqtt_nvs", "-Ifirmware/tests/mqtt_sdk"]
        sources = ["firmware/tests/mqtt_sdk/tls_fake.cpp"]
    elif name in ("mqtt_tls_test", "mqtt_client_config_test"):
        flags, sources = ["-Ifirmware/tests/mqtt_sdk"], TLS
        if name == "mqtt_client_config_test":
            variants = [("_native", []), ("_hook", ["-DMQTT_TEST_NO_DATE"]),
                        ("_unsupported", ["-DMQTT_TEST_NO_DATE", "-DMQTT_TEST_NO_HOOK"])]
    else:
        flags, sources = ["-Ifirmware/tests/pump"], OWNER + BRIDGE
    if name in ("autonomous_owner_test", "autonomous_storage_test", "autonomous_watering_test", "autonomous_lease_rejection_test"):
        variants = [("", []), ("_demo", ["-DWATERING_AUTONOMOUS_TEST=1", "-DWATERING_DEMO_RETRY=1"])]
    if "#include <openssl/" in path.read_text():
        libs = ["-lcrypto"]
    for suffix, extra in variants:
        yield name + suffix, flags + extra, sources, libs


def main():
    out = Path(tempfile.mkdtemp(prefix="watering-host-"))
    print("ARTIFACTS:", out, flush=True)
    jobs = []
    for path in sorted((ROOT / "firmware/tests").rglob("*_test.cpp")):
        for name, flags, sources, libs in specs(path):
            jobs.append((name, flags, sources, str(path.relative_to(ROOT)), libs))
    jobs.append(("legacy_explicit_flag0", ["-DWATERING_AUTONOMOUS_TEST=0", "-Ifirmware/tests/pump"],
                 OWNER + [P + "autonomous_owner.cpp"], "firmware/tests/pump/cooldown_test.cpp", []))
    results = []
    for name, flags, sources, test, libs in jobs:
        exe = out / (name + ".exe")
        cmd = ["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror"] + flags + sources + [test] + libs + ["-o", str(exe)]
        build = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True)
        (out / (name + ".build.log")).write_text(build.stdout + build.stderr)
        result = dict(name=name, command=cmd, build=build.returncode, run=None)
        if build.returncode == 0:
            scenarios = [None]
            if name == "pump_sdk_test_flag0":
                scenarios = ["success"]
            elif name == "pump_sdk_test_flag1":
                scenarios = ["success", "new_error", "new_null", "register_error", "get_error",
                             "get_null", "shared", "iram_error", "alarm_error", "enable_error",
                             "enable_partial", "start_error", "start_partial", "code_external",
                             "data_external", "time_external", "timer_external", "interrupt_external",
                             "cleanup_delete", "cleanup_stop", "cleanup_disable"]
            runs, output = [], []
            for scenario in scenarios:
                args = [str(exe)] + ([] if scenario is None else [scenario])
                run = subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=30)
                runs.append(dict(scenario=scenario, exit=run.returncode))
                output.append(run.stdout + run.stderr)
            result["run"] = 0 if all(r["exit"] == 0 for r in runs) else 1
            result["scenarios"] = runs
            (out / (name + ".run.log")).write_text("".join(output))
        print(name, "PASS" if result["run"] == 0 else "FAIL", flush=True)
        results.append(result)
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1")
    python = subprocess.run([sys.executable, "-B", "-m", "unittest", "discover", "-s", "firmware/tests", "-p", "*_test.py"],
                            cwd=ROOT, env=env, capture_output=True, text=True)
    (out / "python.log").write_text(python.stdout + python.stderr)
    print(python.stdout + python.stderr, flush=True)
    passed = sum(r["run"] == 0 for r in results)
    (out / "results.json").write_text(json.dumps(dict(cpp=results, python_exit=python.returncode), indent=2))
    print(f"CPP {passed}/{len(results)} PASS; Python exit={python.returncode}", flush=True)
    return 0 if passed == len(results) and python.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
