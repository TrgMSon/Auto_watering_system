// Host regression tests: compile sensors.cpp with these Arduino/DHT substitutes.
#include "Arduino.h"
#include "../esp32/sensors.h"
#include <cmath>
#include <iostream>
#include <limits>

uint32_t fakeNow = 0, fakeEcho = 5831;
int fakeAdc = 2630, fakeEchoLevel = LOW, adcCalls = 0, pulseCalls = 0;
int dhtCalls = 0;
float fakeTemperature = 25, fakeHumidity = 60;

int main() {
  sensors::Reading r;
  sensors::begin();
  assert(!sensors::poll(r));
  fakeNow = 1999;
  assert(!sensors::poll(r));
  fakeNow = 2000;
  assert(sensors::poll(r));
  assert(adcCalls == 20 && dhtCalls == 1);
  assert(r.soilValid && r.soilRawAdc == 2630 && r.soilPercent == 50);
  assert(r.dhtValid && r.temperatureC == 25 && r.humidityPercent == 60);
  assert(r.distanceValid && std::fabs(r.distanceCm - 100) < 0.1f);
  assert(!sensors::poll(r));
  auto next = [&]() { fakeNow += 2000; assert(sensors::poll(r)); };
  fakeAdc = 3230; next(); assert(r.soilPercent == 0);
  fakeAdc = 2030; next(); assert(r.soilPercent == 100);
  fakeAdc = 3500; next(); assert(r.soilValid && r.soilPercent == 0);
  fakeAdc = 1500; next(); assert(r.soilValid && r.soilPercent == 100);
  for (int raw : {0, 4095, -1, 4096}) {
    fakeAdc = raw; next(); assert(!r.soilValid && std::isnan(r.soilPercent));
  }
  for (float temperature : {NAN, INFINITY, -1.0f, 51.0f}) {
    fakeTemperature = temperature; next();
    assert(!r.dhtValid && std::isnan(r.temperatureC) && std::isnan(r.humidityPercent));
  }
  fakeTemperature = 25;
  for (float humidity : {NAN, INFINITY, -1.0f, 101.0f}) {
    fakeHumidity = humidity; next(); assert(!r.dhtValid);
  }
  for (uint32_t echo : {0u, 1u, 30000u}) {
    fakeEcho = echo; next(); assert(!r.distanceValid && std::isnan(r.distanceCm));
    assert(r.echoUs == echo);
  }
  const int previousPulses = pulseCalls;
  fakeEchoLevel = HIGH; next();
  assert(!r.distanceValid && r.echoUs == 0 && pulseCalls == previousPulses);
  fakeEchoLevel = LOW; fakeEcho = 5831; fakeAdc = 2630; fakeHumidity = 60;
  next(); assert(r.soilValid && r.dhtValid && r.distanceValid);
  fakeNow = std::numeric_limits<uint32_t>::max() - 999;
  sensors::begin(); fakeNow = 999; assert(!sensors::poll(r));
  fakeNow = 1000; assert(sensors::poll(r));
  std::cout << "PASS: scheduling, rollover, conversion, failure/recovery, sensor-only GPIO\n";
}
