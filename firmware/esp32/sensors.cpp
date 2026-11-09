#include "sensors.h"

#include <Arduino.h>
#include <DHT.h>

#include "board_config.h"

namespace sensors {
namespace {
constexpr uint32_t kIntervalMs = 2000;  // Also allows DHT11 startup settling.
constexpr unsigned kSoilSamples = 20;
constexpr int kAdcMax = 4095;
constexpr uint32_t kEchoTimeoutUs = 30000;
// Provisional SRF05 operating envelope; verify against the installed module.
constexpr float kMinDistanceCm = 2.0f;
constexpr float kMaxDistanceCm = 450.0f;
DHT dht(board_config::kDht11DataPin, DHT11);
uint32_t lastAcquisitionMs = 0;

void readSoil(Reading& reading) {
  uint32_t sum = 0;
  bool samplesValid = true;
  for (unsigned i = 0; i < kSoilSamples; ++i) {
    const int raw = analogRead(board_config::kSoilMoistureAoPin);
    if (raw < 0 || raw > kAdcMax) {
      samplesValid = false;
    } else {
      sum += raw;
    }
  }
  if (!samplesValid) return;
  reading.soilRawAdc = static_cast<float>(sum) / kSoilSamples;
  // Rails are suspect (short/disconnect/saturation), not valid dry/wet values.
  if (sum == 0 || sum == kSoilSamples * kAdcMax ||
      board_config::kSoilDryAdc <= board_config::kSoilWetAdc) return;
  const float percent = 100.0f * (board_config::kSoilDryAdc - reading.soilRawAdc) /
                        (board_config::kSoilDryAdc - board_config::kSoilWetAdc);
  reading.soilPercent = constrain(percent, 0.0f, 100.0f);
  reading.soilValid = true;
}

void readDht(Reading& reading) {
  // The second call uses the library's cached frame, not a second transaction.
  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();
  reading.dhtValid = isfinite(temperature) && isfinite(humidity) &&
                     temperature >= 0.0f && temperature <= 50.0f &&
                     humidity >= 0.0f && humidity <= 100.0f;
  if (reading.dhtValid) {
    reading.temperatureC = temperature;
    reading.humidityPercent = humidity;
  }
}

void readDistance(Reading& reading) {
  // Reject stuck-high ECHO before issuing a trigger.
  if (digitalRead(board_config::kSrf05EchoPin) != LOW) return;
  digitalWrite(board_config::kSrf05TrigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(board_config::kSrf05TrigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(board_config::kSrf05TrigPin, LOW);
  reading.echoUs = pulseIn(board_config::kSrf05EchoPin, HIGH, kEchoTimeoutUs);
  if (reading.echoUs == 0) return;
  const float distance = reading.echoUs * 0.0343f / 2.0f;
  if (!isfinite(distance) || distance < kMinDistanceCm || distance > kMaxDistanceCm) return;
  reading.distanceCm = distance;
  reading.distanceValid = true;
}
}  // namespace

void begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(board_config::kSoilMoistureAoPin, ADC_11db);
  pinMode(board_config::kSoilMoistureAoPin, INPUT);
  digitalWrite(board_config::kSrf05TrigPin, LOW);
  pinMode(board_config::kSrf05TrigPin, OUTPUT);
  pinMode(board_config::kSrf05EchoPin, INPUT);
  dht.begin();
  lastAcquisitionMs = millis();
  // Deliberately no access to the relay pin.
}

bool poll(Reading& reading) {
  const uint32_t now = millis();
  if (static_cast<uint32_t>(now - lastAcquisitionMs) < kIntervalMs) return false;
  lastAcquisitionMs = now;
  reading = Reading{};  // Never retain stale valid data after a failure.
  reading.sampledAtMs = now;
  readSoil(reading);
  readDht(reading);
  readDistance(reading);
  return true;
}
}  // namespace sensors
