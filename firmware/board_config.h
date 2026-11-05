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

// Relay polarity, supply voltages and transistor wiring are UNCONFIRMED.
// Do not configure the relay pin as OUTPUT or drive it before confirmation.
}  // namespace board_config
