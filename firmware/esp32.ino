#include <Arduino.h>

#include "board_config.h"

// Preparation skeleton only: no GPIO, sensors, relay or network initialization.
constexpr char kFirmwareVersion[] = "0.0.1-skeleton";

void setup() {
  Serial.begin(115200);
  Serial.print("Smart Plant Watering firmware ");
  Serial.println(kFirmwareVersion);
  Serial.println("Preparation skeleton started; hardware control is disabled.");
}

void loop() {
  // Yield CPU time without configuring or driving application GPIO.
  delay(1000);
}
