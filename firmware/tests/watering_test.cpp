#include "../esp32/watering.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace watering;
sensors::Reading reading(uint32_t now, float soil = 20, float distance = 50) {
  sensors::Reading r;
  r.sampledAtMs = now; r.soilValid = true; r.soilPercent = soil;
  r.distanceValid = true; r.distanceCm = distance;
  return r;  // DHT invalid deliberately: monitoring only.
}
int main() {
  Config config;
  // Synthetic test geometry only; never used as deployed calibration.
  config.fullDistanceCm = 10; config.emptyDistanceCm = 110;
  config.calibrationConfirmed = true;
  auto r = reading(0);
  Controller missing;
  assert(!missing.evaluate(r, 0).desiredPumpOn);
  for (int mode = 0; mode < 5; ++mode) {
    Config bad = config;
    if (mode == 0) bad.calibrationConfirmed = false;
    if (mode == 1) bad.fullDistanceCm = NAN;
    if (mode == 2) bad.emptyDistanceCm = INFINITY;
    if (mode == 3) bad.emptyDistanceCm = 10;
    if (mode == 4) bad.emptyDistanceCm = 5;
    Controller c(bad); auto d = c.evaluate(r, 0);
    assert(!d.waterValid && std::isnan(d.waterPercent) && !d.desiredPumpOn);
  }
  Controller c(config);
  assert(!c.evaluate(reading(0, 30), 0).desiredPumpOn);
  assert(!c.evaluate(reading(0, 50), 0).desiredPumpOn);
  assert(c.evaluate(r, 0).desiredPumpOn);
  assert(c.evaluate(reading(1000, 30), 1000).desiredPumpOn);
  assert(c.evaluate(reading(2000, 69.9f), 2000).desiredPumpOn);
  assert(!c.evaluate(reading(3000, 70), 3000).desiredPumpOn);
  for (float distance : {89.0f, 90.0f, 91.0f}) {
    Controller water(config); auto d = water.evaluate(reading(0, 20, distance), 0);
    assert(d.waterValid && d.desiredPumpOn == (distance <= 90));
  }
  for (float distance : {1.0f, 200.0f}) {
    Controller water(config); auto d = water.evaluate(reading(0, 20, distance), 0);
    assert(d.waterPercent == (distance == 1 ? 100 : 0));
  }
  for (float soil : {NAN, INFINITY, -1.0f, 101.0f}) {
    Controller invalid(config); assert(!invalid.evaluate(reading(0, soil), 0).desiredPumpOn);
  }
  for (float distance : {NAN, INFINITY, -1.0f, 0.0f}) {
    Controller invalid(config);
    assert(!invalid.evaluate(reading(0, 20, distance), 0).desiredPumpOn);
  }
  for (int fault = 0; fault < 3; ++fault) {
    Controller safety(config); assert(safety.evaluate(r, 0).desiredPumpOn);
    auto bad = reading(1000);
    if (fault == 0) bad.soilValid = false;
    if (fault == 1) bad.distanceValid = false;
    if (fault == 2) bad.distanceCm = 100;
    auto d = safety.evaluate(bad, 1000);
    assert(!d.desiredPumpOn && d.state == State::Locked);
    assert(!safety.evaluate(reading(2000), 2000).desiredPumpOn);
    assert(safety.evaluate(reading(3000, 70), 3000).state == State::Idle);
    assert(safety.evaluate(reading(4000), 4000).desiredPumpOn);
  }
  Controller stale(config);
  assert(stale.evaluate(r, 0).desiredPumpOn);
  assert(stale.evaluate(r, 4000).desiredPumpOn);
  assert(!stale.evaluate(r, 4001).desiredPumpOn);
  assert(stale.evaluate(reading(0, 70), 4500).state == State::Locked);
  Controller timeout(config);
  timeout.evaluate(r, 0);
  assert(timeout.evaluate(reading(4999), 4999).desiredPumpOn);
  auto d = timeout.evaluate(reading(5000), 5000);
  assert(d.state == State::Cooldown && d.reason == Reason::Timeout && !d.desiredPumpOn);
  assert(timeout.evaluate(reading(6000), 6000).state == State::Cooldown);
  assert(timeout.evaluate(reading(35000, 70), 35000).state == State::Idle);
  Controller rollover(config);
  const uint32_t start = std::numeric_limits<uint32_t>::max() - 1000;
  assert(rollover.evaluate(reading(start), start).desiredPumpOn);
  assert(rollover.evaluate(reading(3998), 3998).desiredPumpOn);
  assert(!rollover.evaluate(reading(3999), 3999).desiredPumpOn);
  Controller freshWrap(config);
  assert(freshWrap.evaluate(reading(start), 500).desiredPumpOn);
  Config badConfig = config; badConfig.startPercent = 70;
  assert(!Controller(badConfig).evaluate(r, 0).desiredPumpOn);
  sensors::Reading absent;
  assert(!Controller(config).evaluate(absent, 0).desiredPumpOn);
  std::cout << "PASS: thresholds, hysteresis, calibration, safety, freshness, lock, timeout, rollover, DHT independent\n";
}
