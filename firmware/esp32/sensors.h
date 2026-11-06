#pragma once

#include <stdint.h>
#include <math.h>

namespace sensors {
struct Reading {
  uint32_t sampledAtMs = 0;
  float soilRawAdc = NAN;
  float soilPercent = NAN;
  bool soilValid = false;
  float temperatureC = NAN;
  float humidityPercent = NAN;
  bool dhtValid = false;
  uint32_t echoUs = 0;
  float distanceCm = NAN;
  bool distanceValid = false;
};

void begin();
// Returns true only when a new complete acquisition is available.
bool poll(Reading& reading);
}  // namespace sensors
