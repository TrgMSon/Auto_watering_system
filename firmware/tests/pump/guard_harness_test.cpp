#include "Arduino.h"
#include "../../esp32/guard_test_harness.h"
#include <iostream>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
sensors::Reading sample() {
  sensors::Reading r; r.sampledAtMs = fakeNow;
  r.soilValid = r.distanceValid = true; r.soilPercent = 20; r.distanceCm = 5;
  return r;
}
int main() {
  pump::testGuardEnable(true);
  for (bool wrap : {false, true}) {
    fakeNow = wrap ? UINT32_MAX - 100 : 0;
    irrigation::System s; s.begin(); guard_test::Harness h;
    assert(h.start(s) == irrigation::Result::Invalid);
    assert(h.activate(s));
    assert(h.start(s) == irrigation::Result::Unsafe && !h.starving());
    s.update(sample()); assert(h.start(s) == irrigation::Result::Invalid);
    assert(h.activate(s)); h.disable(); assert(h.start(s) == irrigation::Result::Invalid);
    assert(h.activate(s)); assert(h.start(s) == irrigation::Result::Accepted);
    assert(h.started.durationMs == 5000 && h.waiting());
    fakeNow += 249; pump::testGuardInterrupt(); assert(outputLevel == HIGH);
    fakeNow += 1; pump::testGuardInterrupt(); assert(outputLevel == LOW);
    assert(h.waiting()); h.recover(s); assert(h.starving());
    fakeNow += 500; assert(!h.waiting()); h.recover(s);
    assert(!h.starving() && !h.recovery.pumpOn);
    assert(h.stopped.cause == pump::StopCause::SafetyLeaseExpired);
    assert(h.stopped.generation == h.started.generation);
    assert(h.recovery.decision.state == watering::State::Locked);
    assert(h.recovery.decision.reason == watering::Reason::SafetyLeaseExpired);
    assert(pump::takeStopEvent().cause == pump::StopCause::None);
    assert(h.activate(s)); assert(h.start(s) == irrigation::Result::Locked);
    s.command("AUTO"); assert(!h.activate(s));
  }
  std::cout << "PASS: harness opt-in, one-shot, validation, starvation, recovery, rollover\n";
}
