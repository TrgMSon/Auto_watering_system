#include <Arduino.h>

#include "board_config.h"
#include "sensors.h"
#include "pump.h"
#include "pump_serial.h"
#include "irrigation.h"
#include "preferences_storage.h"
#include "ble_server.h"
#include "mqtt_service.h"
#include "ble_pairing.h"
#include "ble_command_bridge.h"
#include "ble_key_nvs.h"
#include <esp_mac.h>
static ble_keys::KeySource bleKeySource;
static ble_commands::SystemClock bleClock;
#include "guard_test_harness.h"
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
static guard_test::Harness guardTest;
#endif
static configuration::PreferencesStorage configStorage;

constexpr char kFirmwareVersion[] = "0.0.6-persistent-config";
static pump::SerialCommands commands;
static irrigation::System irrigationSystem;

void printPumpStatus() {
  const auto& status = irrigationSystem.status();
  Serial.printf("mode=%s pump=%s watering_state=%s soil_pct=%.2f soil_valid=%d "
                "water_pct=%.2f water_valid=%d reason=%s remaining_ms=%lu "
                "cooldown_remaining_ms=%lu watering_attempts=%u\n",
                irrigation::name(status.mode), status.pumpOn ? "ON" : "OFF",
                watering::name(status.decision.state), irrigationSystem.reading().soilPercent,
                status.soilValid, status.decision.waterPercent, status.decision.waterValid,
                watering::name(status.decision.reason), static_cast<unsigned long>(status.remainingMs),
                static_cast<unsigned long>(status.decision.cooldownRemainingMs), status.decision.wateringAttempts);
}

void servicePump() {
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  ble_command_bridge::serviceOwner(irrigationSystem, &bleClock);
#endif
  irrigationSystem.service();
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  static uint32_t lastSnapshotMs = 0, lastSampleMs = 0;
  static bool lastActuatorOn = false;
  const uint32_t now = millis();
  const bool actuatorOn = pump::isOn();
  const auto& reading = irrigationSystem.reading();
  if (actuatorOn != lastActuatorOn || reading.sampledAtMs != lastSampleMs ||
      static_cast<uint32_t>(now - lastSnapshotMs) >= 100) {
    plant_ble::publish(reading, actuatorOn, now);
    plant_mqtt::publish(reading);
    lastSnapshotMs = now; lastSampleMs = reading.sampledAtMs; lastActuatorOn = actuatorOn;
  }
#endif
}

void setup() {
  irrigationSystem.begin(&configStorage);
  Serial.begin(115200);
  Serial.print("Smart Plant Watering firmware ");
  Serial.println(kFirmwareVersion);
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
  Serial.println("WARNING F3.1d TEST BUILD: GUARD_TEST_ENABLE then GUARD_TEST_STALL; GUARD_TEST_DISABLE disarms. Reflash production to remove harness.");
#endif
  Serial.println("F2.4: CONFIG / SET / RESET_CONFIG / ON / OFF / STATUS / AUTO / MANUAL / RESET_WATERING (newline); boot MANUAL/OFF.");
  Serial.println("BLE_PAIR / BLE_PAIR_CLOSE: local USB pairing window; open requires MANUAL/OFF (60s)");
  printPumpStatus();
  sensors::begin();
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  ble_keys::NvsReader keyStorage;
  uint8_t staMac[6];
  ble_keys::Status keyStatus = ble_keys::Status::Invalid;
  if (esp_read_mac(staMac, ESP_MAC_WIFI_STA) == ESP_OK)
    keyStatus = bleKeySource.initialize(keyStorage, staMac);
  Serial.println(keyStatus == ble_keys::Status::Loaded ? "BLE key loaded" :
      keyStatus == ble_keys::Status::Missing ? "BLE key missing" : "BLE key invalid");
  if (!plant_ble::begin(&bleKeySource)) Serial.println("BLE worker unavailable; irrigation unchanged");
  plant_mqtt::begin();
#endif
}

void loop() {
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
  if (guardTest.starving()) {
    if (guardTest.waiting()) { delay(1); return; }
    guardTest.recover(irrigationSystem);
    Serial.printf("GUARD_TEST_START started_ms=%lu duration_ms=%lu generation=%lu\n",
        static_cast<unsigned long>(guardTest.started.startedMs),
        static_cast<unsigned long>(guardTest.started.durationMs),
        static_cast<unsigned long>(guardTest.started.generation));
    Serial.printf("GUARD_TEST_RECOVERY cause=%s generation=%lu pump=%s state=%s reason=%s\n",
        guardTest.stopped.cause == pump::StopCause::SafetyLeaseExpired ? "SAFETY_LEASE_EXPIRED" :
        guardTest.stopped.cause == pump::StopCause::Timeout ? "TIMEOUT" : "OTHER_OR_NONE",
        static_cast<unsigned long>(guardTest.stopped.generation),
        guardTest.recovery.pumpOn ? "ON" : "OFF", watering::name(guardTest.recovery.decision.state),
        watering::name(guardTest.recovery.decision.reason));
    return;
  }
#endif
  servicePump();
  // Bound per-loop work even under a continuous Serial stream.
  for (unsigned i = 0; i < 32 && Serial.available() > 0; ++i) {
    const char* line = nullptr;
    if (commands.feedLine(static_cast<char>(Serial.read()), line)) {
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
      if (line && strcmp(line, "GUARD_TEST_ENABLE") == 0) {
        Serial.println(guardTest.activate(irrigationSystem) ? "GUARD_TEST_ARMED_ONE_SHOT" : "GUARD_TEST_REJECTED");
        return;
      }
      if (line && strcmp(line, "GUARD_TEST_DISABLE") == 0) {
        guardTest.disable(); Serial.println("GUARD_TEST_DISABLED"); return;
      }
      if (line && strcmp(line, "GUARD_TEST_STALL") == 0) {
        const auto result = guardTest.start(irrigationSystem);
        // No logging between successful start and starvation: Serial can block.
        if (guardTest.starving()) return;
        Serial.println(irrigation::name(result)); return;
      }
#endif
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
      const auto pairingCommand=ble_pairing::command(line);
      if(pairingCommand!=ble_pairing::Command::None){
        const bool queued=plant_ble::requestPairing(pairingCommand==ble_pairing::Command::Open,
            irrigationSystem.status().mode==irrigation::Mode::Manual,pump::isOn());
        Serial.println(queued?"BLE_PAIR_REQUEST_QUEUED":"BLE_PAIR_REQUEST_REJECTED (requires MANUAL/OFF for open and available BLE queue)");
        continue;
      }
#endif
      const auto result = irrigationSystem.command(line);
      Serial.println(irrigation::name(result));
      if (line && strcmp(line, "CONFIG") == 0) {
        const auto& settings = irrigationSystem.configurationStatus();
        const auto& c = settings.active();
        Serial.printf("soil_start_pct=%.2f soil_stop_pct=%.2f water_min_pct=%.2f "
                      "full_distance_cm=%.2f empty_distance_cm=%.2f max_run_seconds=%lu source=%s schema=%lu\n",
                      c.soilStart, c.soilStop, c.waterMin, c.fullCm, c.emptyCm,
                      static_cast<unsigned long>(c.maxRunSeconds),
                      settings.persistedSchema() == 1 ? "LEGACY_LOADED_MIGRATION_PENDING" :
                      settings.persisted() ? "NVS_PERSISTED" : "DEFAULTS_NOT_PERSISTED",
                      static_cast<unsigned long>(settings.persistedSchema()));
      }
      printPumpStatus();
    }
    servicePump();
  }
  sensors::Reading reading;
  // Leave a guard window for the bounded sensor transaction near pump expiry.
  const uint32_t remaining = irrigationSystem.status().remainingMs;
  if ((!remaining || remaining > 250) && sensors::poll(reading)) {
    irrigationSystem.update(reading);  // Stop on new unsafe data before logging.
    servicePump();
    Serial.printf("t=%lu ms soil_raw=%.2f soil_pct=%.2f soil_valid=%d "
                  "temp_C=%.2f humidity_pct=%.2f dht_valid=%d "
                  "echo_us=%lu distance_cm=%.2f distance_valid=%d\n",
                  static_cast<unsigned long>(reading.sampledAtMs),
                  reading.soilRawAdc, reading.soilPercent, reading.soilValid,
                  reading.temperatureC, reading.humidityPercent, reading.dhtValid,
                  static_cast<unsigned long>(reading.echoUs),
                  reading.distanceCm, reading.distanceValid);
  }
  servicePump();
  static uint32_t lastReportMs = 0;
  static bool lastOn = false;
  static watering::State lastState = watering::State::Idle;
  const auto& status = irrigationSystem.status();
  const uint32_t now = millis();
  if (status.pumpOn != lastOn || status.decision.state != lastState ||
      static_cast<uint32_t>(now - lastReportMs) >= 2000) {
    printPumpStatus();
    lastReportMs = now;
    lastOn = status.pumpOn;
    lastState = status.decision.state;
  }
  delay(1);  // Yield without a long scheduling delay.
}
