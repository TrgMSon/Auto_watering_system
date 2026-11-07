#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include <iostream>
#include <limits>
#include <vector>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
using irrigation::Result;
using watering::State;
struct Store : configuration::Storage {
  std::vector<uint8_t> data;
  bool read(uint8_t* b, size_t cap, size_t& n) override {
    n = data.size(); if (!n || n > cap) return false;
    memcpy(b, data.data(), n); return true;
  }
  bool write(const uint8_t* b, size_t n) override { data.assign(b,b+n); return true; }
};
sensors::Reading sample() {
  sensors::Reading r; r.sampledAtMs = fakeNow;
  r.soilValid = r.distanceValid = true; r.soilPercent = 20; r.distanceCm = 5;
  return r;  // DHT invalid must remain monitoring-only.
}
int main() {
  irrigation::System production; production.begin(); production.update(sample());
  assert(pump::QualificationPolicy{}.ceilingSeconds() == 5);
  assert(production.startManual(6) == Result::QualificationLimit);
  assert(pump::start(6) == pump::Result::QualificationLimit); // Independent actuator gate.
  for (uint32_t invalid : {0u, 601u, UINT32_MAX})
    assert(production.startManual(invalid) == Result::InvalidDuration);
  Store store;
  irrigation::System s(irrigation::confirmedConfig(), pump::QualificationPolicy::forTests(600));
  s.begin(&store); s.update(sample());
  assert(s.startManual(121) == Result::ExceedsConfiguredMaximum);
  auto config = s.configurationStatus().active(); config.maxRunSeconds = 600;
  assert(s.updateConfiguration(config) == configuration::Result::Saved);
  for (uint32_t seconds : {1u, 5u, 60u, 120u, 600u}) for (bool wrap : {false, true}) {
    fakeNow = wrap ? UINT32_MAX - 500 : 0;
    s.begin(&store); s.update(sample());
    const uint32_t start = fakeNow;
    assert(s.startManual(seconds) == Result::Accepted);
    assert(pump::run().startedMs == start && pump::run().durationMs == seconds*1000);
    const auto generation = pump::run().generation;
    for (uint32_t elapsed = 0; elapsed < seconds*1000; elapsed += 500) {
      fakeNow = start + elapsed; s.update(sample());
      assert(s.status().pumpOn && s.status().decision.state == State::Watering);
      assert(s.startManual(1) == Result::AlreadyOn);
      assert(s.startManual(600) == Result::AlreadyOn);
      assert(pump::run().generation == generation);
    }
    fakeNow = start + seconds*1000 - 1; s.update(sample());
    assert(s.status().remainingMs == 1);
    fakeNow += 1;
    // An accessor stops GPIO but does not erase the timeout cause.
    assert(pump::remainingMs() == 0);
    assert(s.startManual(seconds) == Result::AlreadyOn);
    assert(s.status().decision.state == State::Cooldown);
    assert(s.status().decision.wateringAttempts == 0);
    const auto cooldown = s.status().decision.cooldownRemainingMs;
    assert(cooldown == 30000 && pump::takeStopEvent().cause == pump::StopCause::None);
    fakeNow += 100; s.update(sample());
    assert(s.status().decision.cooldownRemainingMs == cooldown - 100);
    assert(s.startManual(1) == Result::Cooldown);
  }
  for (int fault = 0; fault < 4; ++fault) {
    s.begin(&store); s.update(sample()); assert(s.startManual(600) == Result::Accepted);
    fakeNow += 1000; auto bad = sample();
    if (fault == 0) bad.soilValid = false;
    if (fault == 1) bad.distanceValid = false;
    if (fault == 2) bad.distanceCm = 12.26f;
    if (fault == 3) bad.sampledAtMs -= 4001;
    s.update(bad); assert(!s.status().pumpOn && s.status().decision.state == State::Locked);
    s.update(sample()); assert(s.startManual(1) == Result::Locked);
    s.begin(&store); s.update(bad); assert(s.startManual(1) == Result::Unsafe);
  }
  s.begin(&store); s.update(sample()); s.startManual(120);
  fakeNow += 4001; s.service();
  assert(!s.status().pumpOn && s.status().decision.state == State::Locked);
  s.begin(&store); s.update(sample()); s.startManual(60); s.command("OFF");
  assert(!s.status().pumpOn && s.status().decision.state == State::Locked);
  assert(pump::takeStopEvent().cause == pump::StopCause::None);
  s.begin(&store); s.update(sample()); s.startManual(60); pump::command("OFF"); s.service();
  assert(s.status().decision.reason == watering::Reason::ActuatorStopped);
  assert(pump::takeStopEvent().cause == pump::StopCause::None);
  s.begin(&store); s.update(sample()); s.startManual(1);
  fakeNow += 1000; auto bad = sample(); bad.distanceValid = false; s.update(bad);
  assert(s.status().decision.state == State::Locked &&
      s.status().decision.reason == watering::Reason::DistanceInvalid);
  s.begin(&store); s.update(sample()); s.command("ON");
  assert(pump::run().durationMs == 5000);
  s.begin(&store); s.update(sample()); s.command("AUTO");
  assert(pump::run().durationMs == 5000 && s.startManual(1) == Result::ManualOnly);
  s.begin(&store);
  assert(!s.status().pumpOn && s.status().mode == irrigation::Mode::Manual);
  production.begin(); production.update(sample());
  assert(pump::start(60) == pump::Result::QualificationLimit); // Boot restores production policy.
  std::cout << "PASS: typed durations, qualification, deadlines, rollover, safety and reconciliation\n";
}
