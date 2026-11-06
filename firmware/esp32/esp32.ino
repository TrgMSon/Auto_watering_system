#include <Arduino.h>

#include "board_config.h"
#include "sensors.h"
#include "pump.h"
#include "pump_serial.h"

constexpr char kFirmwareVersion[] = "0.0.3-pump-test";
static pump::SerialCommands commands;

void printPumpStatus() {
  const uint32_t remaining = pump::remainingMs();
  Serial.printf("pump=%s remaining_ms=%lu\n", remaining ? "ON" : "OFF",
                static_cast<unsigned long>(remaining));
}

void servicePump() {
  if (pump::poll()) {
    Serial.println("pump timeout OFF");
    printPumpStatus();
  }
}

void setup() {
  pump::begin();
  Serial.begin(115200);
  Serial.print("Smart Plant Watering firmware ");
  Serial.println(kFirmwareVersion);
  Serial.println("F2.2 manual test: ON / OFF / STATUS (newline), max 5000 ms. No automatic watering.");
  printPumpStatus();
  sensors::begin();
}

void loop() {
  servicePump();
  // Bound per-loop work even under a continuous Serial stream.
  for (unsigned i = 0; i < 32 && Serial.available() > 0; ++i) {
    pump::Result result;
    if (commands.feed(static_cast<char>(Serial.read()), result)) {
      if (result == pump::Result::Invalid) Serial.println("Invalid command; use ON, OFF, STATUS");
      if (result == pump::Result::AlreadyOn) Serial.println("ON ignored: existing run not extended");
      printPumpStatus();
    }
    servicePump();
  }
  sensors::Reading reading;
  // Leave a guard window for the bounded sensor transaction near pump expiry.
  const uint32_t remaining = pump::remainingMs();
  if ((!remaining || remaining > 250) && sensors::poll(reading)) {
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
  delay(1);  // Yield without a long scheduling delay.
}
