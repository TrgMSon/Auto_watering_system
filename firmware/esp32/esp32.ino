#include <Arduino.h>

#include "board_config.h"
#include "firmware_version.h"
#include "sensors.h"
#include "pump.h"
#include "pump_serial.h"
#include "irrigation.h"
#include "preferences_storage.h"
#include "ble_server.h"
#include "mqtt_service.h"
#include "ota_service.h"
#include "ble_pairing.h"
#include "ble_command_bridge.h"
#include "ble_key_nvs.h"
#include "ble_wifi_service.h"
#include <esp_mac.h>
static ble_keys::KeySource bleKeySource;
static ble_commands::SystemClock bleClock;
#include "guard_test_harness.h"
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
static guard_test::Harness guardTest;
#endif
static configuration::PreferencesStorage configStorage;

static pump::SerialCommands commands;
static irrigation::System irrigationSystem;
static bool deferredServicesStarted = false;
static bool mqttConnectedObserved = false;
static uint32_t mqttConnectedSinceMs = 0;
static uint32_t deferredServicesWaitStartedMs = 0;
static constexpr uint32_t kMqttStableBeforeServicesMs = 10000;
static constexpr uint32_t kMqttStartupFallbackMs = 60000;
#if WATERING_AUTONOMOUS_TEST
#include "autonomous_nvs.h"
#include "mqtt_commands.h"
static autonomous_storage::NvsStorage autonomousStorage;
static autonomous_storage::Ledger autonomousLedger(autonomousStorage);
static pump::Result autonomousStartReserved(uint64_t epoch,uint32_t seconds) {
  if(Serial.available()>0)return pump::Result::Off;
  return ble_command_bridge::startReserved(epoch,seconds);
}
static bool autonomousActivationAllowed(uint64_t epoch) {
  // Any queued USB input conservatively defers ON until Serial is processed.
  return Serial.available()==0 && ble_command_bridge::activationAllowed(epoch);
}
#endif

void printPumpStatus() {
  const auto& status = irrigationSystem.status();
#if WATERING_AUTONOMOUS_TEST
  Serial.printf("EXPERIMENTAL autonomous_block=%s guard_init=%s\n",
      watering::name(status.autonomousBlock), pump::guardInitializationStatus());
#endif
  Serial.printf("mode=%s pump=%s watering_state=%s soil_pct=%.2f soil_valid=%d "
                "water_pct=%.2f water_valid=%d reason=%s remaining_ms=%lu "
                "cooldown_remaining_ms=%lu watering_attempts=%u\n",
                irrigation::name(status.mode), status.pumpOn ? "ON" : "OFF",
                watering::name(status.decision.state), irrigationSystem.reading().soilPercent,
                status.soilValid, status.decision.waterPercent, status.decision.waterValid,
                watering::name(status.decision.reason), static_cast<unsigned long>(status.remainingMs),
                static_cast<unsigned long>(status.decision.cooldownRemainingMs), status.decision.wateringAttempts);
  Serial.printf("desired_mode=%s pending_boot_auto=%u mode_persisted=%u mode_storage_error=%u\n",
      irrigation::name(status.desiredMode), status.pendingBootAuto, status.modePersisted, status.modeStorageError);
}

void servicePump() {
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  plant_ota::ownerService(irrigationSystem);
  ble_command_bridge::serviceOwner(irrigationSystem, &bleClock);
#endif
  irrigationSystem.service();
  static bool lastModeStorageError = false;
  if (irrigationSystem.status().modeStorageError && !lastModeStorageError)
    Serial.println("MODE_STORAGE_ERROR: runtime MANUAL/OFF; flash mode unconfirmed, reboot may restore old AUTO");
  lastModeStorageError = irrigationSystem.status().modeStorageError;
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  static uint32_t lastSnapshotMs = 0, lastSampleMs = 0;
  static bool lastActuatorOn = false;
  const uint32_t now = millis();
  const bool actuatorOn = pump::isOn();
  const auto& reading = irrigationSystem.reading();
  if (actuatorOn != lastActuatorOn || reading.sampledAtMs != lastSampleMs ||
      static_cast<uint32_t>(now - lastSnapshotMs) >= 100) {
    const auto snapshot = irrigationSystem.telemetrySnapshot(now);
    plant_ble::publish(snapshot);
    plant_mqtt::publish(snapshot);
    lastSnapshotMs = now; lastSampleMs = reading.sampledAtMs; lastActuatorOn = actuatorOn;
  }
#endif
}

void startDeferredServices() {
  if (deferredServicesStarted) return;
  const uint32_t now = millis();
  if (plant_mqtt::connected()) {
    if (!mqttConnectedObserved) {
      mqttConnectedObserved = true;
      mqttConnectedSinceMs = now;
    }
  } else {
    mqttConnectedObserved = false;
  }
  const bool mqttStable = mqttConnectedObserved &&
      static_cast<uint32_t>(now - mqttConnectedSinceMs) >= kMqttStableBeforeServicesMs;
  const bool startupWaitExpired = static_cast<uint32_t>(now - deferredServicesWaitStartedMs) >=
      kMqttStartupFallbackMs;
  if (!mqttStable && !startupWaitExpired) return;

  if (mqttStable) Serial.println("[BOOT] MQTT stable; starting deferred services");
  else Serial.println("[BOOT] MQTT not connected after startup window; enabling local services and continuing retries");
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
  if (!plant_ota::begin()) Serial.println("[OTA] worker unavailable");
#endif
  deferredServicesStarted = true;
}

void setup() {
#if WATERING_AUTONOMOUS_TEST
  irrigationSystem.setActivationGate(plant_mqtt::commandIngressEpoch,autonomousActivationAllowed,autonomousStartReserved);
  irrigationSystem.begin(&configStorage,&configStorage,&autonomousLedger);
#else
  irrigationSystem.begin(&configStorage, &configStorage);
#endif
  Serial.begin(115200);
#if WATERING_AUTONOMOUS_TEST
  Serial.println("WARNING EXPERIMENTAL AUTONOMOUS TEST BUILD: NVS budget required; not hardware-qualified");
#endif
  Serial.print("Smart Plant Watering firmware ");
  Serial.println(kFirmwareVersion);
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
  Serial.println("WARNING F3.1d TEST BUILD: GUARD_TEST_ENABLE then GUARD_TEST_STALL; GUARD_TEST_DISABLE disarms. Reflash production to remove harness.");
#endif
  Serial.println("F2.4: CONFIG / SET / RESET_CONFIG / ON / OFF / STATUS / AUTO / MANUAL / RESET_WATERING (newline); boot MANUAL/OFF.");
  Serial.println("BLE_PAIR / BLE_PAIR_CLOSE: local USB pairing window; open requires MANUAL/OFF (60s)");
  printPumpStatus();
#if !defined(F31D_GUARD_TEST) || F31D_GUARD_TEST != 1
  if (!ble_wifi_service::begin()) Serial.println("[WIFI] runtime worker unavailable");
  const bool mqttStarted = plant_mqtt::begin();
  if (!mqttStarted)
    Serial.println("[MQTT] worker unavailable (TLS date verification unsupported or task creation failed)");
  plant_ota::confirmBoot();
  deferredServicesWaitStartedMs = millis();
  if (!mqttStarted) deferredServicesWaitStartedMs -= kMqttStartupFallbackMs;
#else
  sensors::begin();
  deferredServicesStarted = true;
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
  startDeferredServices();
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
        const bool queued=!irrigationSystem.otaLocked()&&
            plant_ble::requestPairing(pairingCommand==ble_pairing::Command::Open,
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
  if (deferredServicesStarted && (!remaining || remaining > 250) && sensors::poll(reading)) {
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
