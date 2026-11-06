#include <Arduino.h>

#include "board_config.h"
#include "sensors.h"
#include "pump.h"
#include "pump_serial.h"
#include "irrigation.h"

constexpr char kFirmwareVersion[] = "0.0.5-cooldown-auto";
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
  irrigationSystem.service();
}

void setup() {
  irrigationSystem.begin();
  Serial.begin(115200);
  Serial.print("Smart Plant Watering firmware ");
  Serial.println(kFirmwareVersion);
  Serial.println("F2.3c: ON / OFF / STATUS / AUTO / MANUAL / RESET_WATERING (newline); boot MANUAL/OFF.");
  printPumpStatus();
  sensors::begin();
}

void loop() {
  servicePump();
  // Bound per-loop work even under a continuous Serial stream.
  for (unsigned i = 0; i < 32 && Serial.available() > 0; ++i) {
    const char* line = nullptr;
    if (commands.feedLine(static_cast<char>(Serial.read()), line)) {
      const auto result = irrigationSystem.command(line);
      Serial.println(irrigation::name(result));
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
