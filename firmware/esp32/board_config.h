#pragma once

#include <stdint.h>

// Recovered from project history supplied by the maintainer.
// PROVISIONAL: not fully verified against the physical wiring diagram.
// Pin declarations do not configure or drive any GPIO.
namespace board_config {
constexpr uint8_t kSoilMoistureAoPin = 34;
constexpr uint8_t kDht11DataPin = 27;
constexpr uint8_t kSrf05TrigPin = 18;
constexpr uint8_t kSrf05EchoPin = 19;
constexpr uint8_t kRelayIn1Pin = 23;

// Provisional raw ADC calibration; verify before using for control.
constexpr int kSoilDryAdc = 3230;
constexpr int kSoilWetAdc = 2030;

// GPIO23 -> 1k resistor -> 2N2222 base; emitter GND, collector relay IN1.
// test_relay_1.ino establishes GPIO HIGH = ON, LOW = OFF through this stage.
// Startup/reset fail-safe still requires suitable external hardware bias.
}  // namespace board_config
