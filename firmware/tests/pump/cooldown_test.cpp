#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include "../../esp32/pump.h"
#include "../../esp32/pump_serial.h"
#include <iostream>
#include <limits>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
using watering::State;
using irrigation::Result;
sensors::Reading sample(float soil = 20, float distance = 5) {
  sensors::Reading r;
  r.sampledAtMs = fakeNow; r.soilValid = true; r.soilPercent = soil;
  r.distanceValid = true; r.distanceCm = distance;
  return r;
}
void check(const irrigation::System& s, State state, unsigned attempts) {
  assert(s.status().decision.state == state);
  assert(s.status().decision.wateringAttempts == attempts);
  const bool on = state == State::Watering;
  assert(s.status().pumpOn == on && pump::isOn() == on);
  assert(outputLevel == (on ? HIGH : LOW));
  assert(s.status().decision.desiredPumpOn == on);
}
int main() {
  irrigation::System s;
  s.begin(); check(s, State::Idle, 0);
  assert(s.status().mode == irrigation::Mode::Manual);
  s.update(sample()); s.command("AUTO"); check(s, State::Watering, 1);
  assert(s.command("RESET_WATERING") == Result::AlreadyOn);
  for (unsigned attempt = 1; attempt <= 3; ++attempt) {
    check(s, State::Watering, attempt);
    fakeNow += 4999; s.update(sample());
    const int before = writes;
    s.command("AUTO"); assert(writes == before && s.status().remainingMs == 1);
    fakeNow += 1;
    // Exercise independent physical cutoff before integration reconciles it.
    assert(pump::poll()); s.update(sample());
    if (attempt == 3) break;
    check(s, State::Cooldown, attempt);
    assert(s.status().decision.cooldownRemainingMs == 30000);
    assert(s.command("RESET_WATERING") == Result::Cooldown);
    s.command("OFF"); check(s, State::Cooldown, attempt);
    assert(s.command("ON") == Result::Cooldown);
    s.command("AUTO"); check(s, State::Cooldown, attempt);
    fakeNow += 29999; s.update(sample());
    assert(s.status().decision.cooldownRemainingMs == 1);
    s.command("AUTO"); check(s, State::Cooldown, attempt);
    fakeNow += 1; s.update(sample());
  }
  check(s, State::Locked, 3);
  assert(s.status().decision.reason == watering::Reason::RetryLimit);
  fakeNow += 30000; s.update(sample()); s.command("AUTO"); check(s, State::Locked, 3);
  s.update(sample(80)); check(s, State::Locked, 3);  // Retry lock needs explicit reset, even wet.
  auto bad = sample(); bad.distanceValid = false; s.update(bad);
  assert(s.command("RESET_WATERING") == Result::Unsafe); check(s, State::Locked, 3);
  s.update(sample()); const int beforeReset = writes;
  assert(s.command("RESET_WATERING") == Result::Accepted);
  check(s, State::Idle, 0); assert(s.status().mode == irrigation::Mode::Manual);
  assert(writes == beforeReset + 1 && outputLevel == LOW);  // OFF only, no hidden ON.
  s.service(); check(s, State::Idle, 0);
  s.command("AUTO"); check(s, State::Watering, 1);

  // At cooldown expiry the whole 30–70 band is OFF; only >=70 completes the cycle.
  for (float soil : {30.0f, 50.0f, 69.9f, 70.0f}) {
    s.begin(); s.update(sample()); s.command("AUTO");
    fakeNow += 5000; s.update(sample()); check(s, State::Cooldown, 1);
    fakeNow += 29999; s.update(sample(soil)); check(s, State::Cooldown, 1);
    fakeNow += 1; s.update(sample(soil)); check(s, State::Idle, soil >= 70 ? 0 : 1);
    s.update(sample()); check(s, State::Watering, soil >= 70 ? 1 : 2);
  }
  // Faults in either WATERING or COOLDOWN latch safety interruption, never resume a partial run.
  for (bool cooling : {false, true}) for (int fault = 0; fault < 6; ++fault) {
    s.begin(); s.update(sample()); s.command("AUTO");
    if (cooling) { fakeNow += 5000; s.update(sample()); }
    auto invalid = sample();
    if (fault == 0) invalid.soilValid = false;
    if (fault == 1) invalid.distanceValid = false;
    if (fault == 2) invalid.sampledAtMs = fakeNow - 4001;
    if (fault == 3) invalid.distanceCm = 12.26f;
    if (fault == 4) invalid.soilPercent = NAN;
    if (fault == 5) invalid.distanceCm = NAN;
    s.update(invalid); check(s, State::Locked, 1);
    fakeNow += 40000; s.update(sample()); s.command("AUTO"); check(s, State::Locked, 1);
    s.update(sample(70)); check(s, State::Idle, 0);  // Existing safety-lock wet rearm retained.
  }
  // Explicit OFF interrupts running work; repeated MANUAL cannot shorten an existing cooldown.
  s.begin(); s.update(sample()); s.command("AUTO"); s.command("OFF");
  check(s, State::Locked, 1); assert(s.status().mode == irrigation::Mode::Manual);
  s.begin(); s.update(sample()); s.command("AUTO"); fakeNow += 5000; s.update(sample());
  s.command("MANUAL"); check(s, State::Cooldown, 1);
  fakeNow += 30000; s.update(sample()); check(s, State::Idle, 1);
  assert(s.status().mode == irrigation::Mode::Manual);
  // Manual intervals do not consume automatic attempts, but also obey cooldown.
  s.begin(); s.update(sample()); s.command("ON"); fakeNow += 5000; s.update(sample());
  check(s, State::Cooldown, 0); assert(s.command("ON") == Result::Cooldown);
  // Cooldown itself crosses millis rollover.
  fakeNow = std::numeric_limits<uint32_t>::max() - 10000;
  s.begin(); s.update(sample()); s.command("AUTO");
  fakeNow += 5000; s.update(sample()); check(s, State::Cooldown, 1);
  fakeNow += 29999; s.update(sample()); check(s, State::Cooldown, 1);
  fakeNow += 1; s.update(sample()); check(s, State::Watering, 2);
  // Full reset command survives the bounded Serial parser.
  s.command("OFF"); pump::SerialCommands parser; const char* line = nullptr;
  for (const char* p = "RESET_WATERING"; *p; ++p) assert(!parser.feedLine(*p, line));
  assert(parser.feedLine('\n', line)); assert(s.command(line) == Result::Accepted);
  check(s, State::Idle, 0);
  // Safety wins over a coincident first timeout; no resumable partial interval.
  s.command("AUTO"); fakeNow += 5000;
  auto unsafeAtTimeout = sample(); unsafeAtTimeout.distanceValid = false;
  s.update(unsafeAtTimeout); check(s, State::Locked, 1);
  assert(s.status().decision.reason == watering::Reason::DistanceInvalid);
  // Wet data at normal cutoff completes the goal without entering retry lock.
  s.begin(); s.update(sample()); s.command("AUTO"); fakeNow += 5000;
  s.update(sample(70)); check(s, State::Idle, 0);
  std::cout << "PASS: F2.3c bounded cooldown/retry, reset, safety interruptions, mode guards, rollover\n";
}
