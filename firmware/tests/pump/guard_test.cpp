#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include <iostream>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
sensors::Reading sample() {
  sensors::Reading r; r.sampledAtMs = fakeNow;
  r.soilValid = r.distanceValid = true; r.soilPercent = 20; r.distanceCm = 5;
  return r;
}
int main() {
  using pump::StopCause;
  pump::testGuardEnable(true);
  for (bool wrap : {false, true}) {
    fakeNow = wrap ? UINT32_MAX - 100 : 0;
    irrigation::System s; s.begin(); s.update(sample());
    assert(s.startManual(5) == irrigation::Result::Accepted);
    const auto generation = pump::run().generation;
    fakeNow += 249; pump::testGuardInterrupt(); assert(outputLevel == HIGH);
    fakeNow += 1; pump::testGuardInterrupt(); assert(outputLevel == LOW);
    // No owner function was called to achieve OFF.
    auto event = pump::takeStopEvent();
    assert(event.cause == StopCause::SafetyLeaseExpired && event.generation == generation);
    assert(!pump::renewSafetyLease(fakeNow, 4000)); // Cannot resurrect a stopped run.
    pump::testGuardInterrupt(); assert(pump::takeStopEvent().cause == StopCause::None);
  }
  irrigation::System s; s.begin(); s.update(sample()); s.startManual(5);
  fakeNow += 251; // Pending ISR: late renewal itself must expire, not extend.
  assert(!pump::renewSafetyLease(fakeNow, 4000) && outputLevel == LOW);
  s.service();
  assert(s.status().decision.state == watering::State::Locked &&
      s.status().decision.reason == watering::Reason::SafetyLeaseExpired);
  assert(pump::takeStopEvent().cause == StopCause::None);
  for (uint32_t duration : {1u,5u}) {
    s.begin(); s.update(sample()); s.startManual(duration);
    const uint32_t start = fakeNow;
    for (uint32_t elapsed=100; elapsed<duration*1000; elapsed+=100) {
      fakeNow = start+elapsed; pump::testGuardInterrupt(); s.update(sample());
      assert(s.status().pumpOn);
    }
    fakeNow = start+duration*1000;
    pump::testGuardInterrupt(); assert(outputLevel == LOW);
    s.update(sample()); assert(s.status().decision.state == watering::State::Cooldown);
    const auto remaining = s.status().decision.cooldownRemainingMs;
    s.service(); assert(s.status().decision.cooldownRemainingMs == remaining);
  }
  // Freshness limits the last renewal, even if the owner keeps servicing.
  s.begin(); const auto reading = sample(); s.update(reading); s.startManual(5);
  const uint32_t start = fakeNow;
  for (unsigned elapsed=100; elapsed<=4000; elapsed+=100) {
    fakeNow = start+elapsed; pump::testGuardInterrupt(); s.service();
    assert(s.status().pumpOn);
  }
  fakeNow = start+4001; pump::testGuardInterrupt(); assert(outputLevel == LOW);
  s.service(); assert(s.status().decision.state == watering::State::Locked);
  // Serial/status-like actuator reads cannot renew a lease.
  s.begin(); s.update(sample()); s.startManual(5);
  for (unsigned i=0;i<5;++i) { fakeNow+=50; pump::remainingMs(); pump::run(); pump::testGuardInterrupt(); }
  assert(outputLevel == LOW);
  // OFF versus pending interrupt, and a delayed old tick after a new generation.
  s.begin(); s.update(sample()); s.startManual(1);
  const auto old = pump::run().generation;
  const auto oldStart = fakeNow;
  s.command("OFF"); pump::testGuardInterrupt(); assert(outputLevel == LOW);
  fakeNow += 100;
  s.begin(); s.update(sample()); s.startManual(5);
  assert(pump::run().generation != old);
  pump::testGuardInterrupt(); assert(outputLevel == HIGH); // Tick uses current state, not old deadline.
  for (unsigned elapsed=200; elapsed<=1000; elapsed+=100) {
    fakeNow = oldStart+elapsed; pump::testGuardInterrupt(); s.update(sample());
    assert(outputLevel == HIGH); // Includes the old generation's scheduled deadline.
  }
  s.command("OFF");
  pump::testGuardFailure(true,false); s.begin(); s.update(sample());
  assert(s.startManual(1) == irrigation::Result::GuardUnavailable && outputLevel == LOW);
  pump::testGuardFailure(false,true); s.begin(); s.update(sample());
  assert(s.startManual(1) == irrigation::Result::GuardUnavailable && outputLevel == LOW);
  pump::testGuardFailure(false,false); s.begin(); s.update(sample());
  assert(s.startManual(1) == irrigation::Result::Accepted);
  std::cout << "PASS: independent guard dispatch, stalled owner, lease, freshness, deadline, rollover, interleavings and failures\n";
}
