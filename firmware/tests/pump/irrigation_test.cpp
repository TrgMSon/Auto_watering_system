#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include "../../esp32/pump.h"
#include "../../esp32/pump_serial.h"
#include <iostream>
#include <limits>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
using irrigation::Mode;
using irrigation::Result;
using watering::State;
sensors::Reading sample(float soil = 20, float distance = 5) {
  sensors::Reading r;
  r.sampledAtMs = fakeNow; r.soilValid = true; r.soilPercent = soil;
  r.distanceValid = true; r.distanceCm = distance;
  return r;  // DHT failure deliberately does not inhibit either mode.
}
void check(const irrigation::System& system, bool on, State state) {
  assert(system.status().pumpOn == on);
  assert(outputLevel == (on ? HIGH : LOW));
  assert(pump::isOn() == on);
  assert(system.status().decision.state == state);
  assert(system.status().decision.desiredPumpOn == on);
}
int main() {
  irrigation::System system;
  system.begin(); check(system, false, State::Idle);
  assert(system.status().mode == Mode::Manual);
  assert(system.command("ON") == Result::Unsafe);
  system.update(sample()); check(system, false, State::Idle);
  assert(system.command("ON") == Result::Accepted); check(system, true, State::Watering);
  fakeNow += 1000; system.update(sample());
  const uint32_t remaining = system.status().remainingMs;
  assert(system.command("ON") == Result::AlreadyOn);
  assert(system.status().remainingMs == remaining);
  assert(system.command("OFF") == Result::Accepted);
  check(system, false, State::Locked); assert(system.status().mode == Mode::Manual);
  assert(system.command("ON") == Result::Locked);
  system.update(sample(70)); check(system, false, State::Idle);
  assert(system.command("ON") == Result::Accepted);  // Wet soil is safe for manual test.
  assert(system.command("MANUAL") == Result::Accepted); check(system, false, State::Locked);
  system.begin(); system.update(sample());
  assert(system.command("AUTO") == Result::Accepted); check(system, true, State::Watering);
  assert(system.status().mode == Mode::Auto);
  assert(system.command("ON") == Result::ManualOnly);
  fakeNow += 1000; system.update(sample());
  const int beforeAuto = writes;
  assert(system.command("AUTO") == Result::Accepted);
  assert(writes == beforeAuto && system.status().remainingMs == 4000);
  system.update(sample(70)); check(system, false, State::Idle);
  system.update(sample(50)); check(system, false, State::Idle);
  system.update(sample()); check(system, true, State::Watering);
  system.command("MANUAL"); check(system, false, State::Locked);
  assert(system.status().mode == Mode::Manual);

  // Every required-data failure rejects manual ON and locks a running AUTO.
  for (int fault = 0; fault < 10; ++fault) {
    system.begin(); auto bad = sample();
    if (fault == 0) bad.soilValid = false;
    if (fault == 1) bad.distanceValid = false;
    if (fault == 2) bad.soilPercent = NAN;
    if (fault == 3) bad.distanceCm = NAN;
    if (fault == 4) bad.distanceCm = 12.26f;
    if (fault == 5) bad.sampledAtMs = fakeNow - 4001;
    if (fault == 6) bad.soilPercent = 101;
    if (fault == 7) bad.distanceCm = INFINITY;
    if (fault == 8) bad.soilPercent = -1;
    if (fault == 9) bad.distanceCm = 0;
    system.update(bad);
    assert(system.command("ON") == Result::Unsafe); check(system, false, State::Idle);
    system.update(sample()); system.command("AUTO"); check(system, true, State::Watering);
    system.update(bad); check(system, false, State::Locked);
    system.update(sample()); system.command("AUTO"); check(system, false, State::Locked);
    system.update(sample(70)); check(system, false, State::Idle);
    system.update(sample()); check(system, true, State::Watering);
    system.command("OFF"); check(system, false, State::Locked);
    // Same safety conditions apply during manual runs as well.
    system.begin(); system.update(sample()); system.command("ON");
    system.update(bad); check(system, false, State::Locked);
  }
  for (int mode = 0; mode < 4; ++mode) {
    auto config = irrigation::confirmedConfig();
    if (mode == 0) config.calibrationConfirmed = false;
    if (mode == 1) config.emptyDistanceCm = config.fullDistanceCm;
    if (mode == 2) config.fullDistanceCm = NAN;
    if (mode == 3) config.emptyDistanceCm = INFINITY;
    irrigation::System uncalibrated(config); uncalibrated.begin(); uncalibrated.update(sample());
    assert(uncalibrated.command("ON") == Result::Unsafe);
    uncalibrated.command("AUTO"); check(uncalibrated, false, State::Idle);
    assert(!uncalibrated.status().decision.waterValid);
  }
  system.begin(); system.update(sample(20, 3.0f));
  assert(system.status().decision.waterPercent == 100);
  system.update(sample(20, 12.26f)); assert(system.status().decision.waterPercent == 0);
  // Exact water safety boundary, using synthetic integer-friendly geometry.
  auto boundaryConfig = irrigation::confirmedConfig();
  boundaryConfig.fullDistanceCm = 10; boundaryConfig.emptyDistanceCm = 110;
  for (float distance : {89.0f, 90.0f, 91.0f}) {
    irrigation::System boundary(boundaryConfig); boundary.begin();
    boundary.update(sample(20, distance));
    assert((boundary.command("ON") == Result::Accepted) == (distance <= 90));
    boundary.command("OFF");
  }
  // Check freshness without any new sensor update.
  system.begin(); system.update(sample()); system.command("AUTO");
  fakeNow += 4001; system.service(); check(system, false, State::Locked);
  assert(system.status().decision.reason == watering::Reason::Stale);

  // F2.3c supersedes first-timeout lock with cooldown; mode toggles cannot bypass it.
  for (bool automatic : {false, true}) {
    system.begin(); system.update(sample()); system.command(automatic ? "AUTO" : "ON");
    const uint32_t start = fakeNow;
    fakeNow = start + 4999; system.update(sample()); check(system, true, State::Watering);
    fakeNow = start + 5000; system.update(sample()); check(system, false, State::Cooldown);
    assert(system.status().decision.reason == watering::Reason::Timeout);
    system.command("AUTO"); check(system, false, State::Cooldown);
    system.command("MANUAL"); assert(system.command("ON") == Result::Cooldown);
    system.command("AUTO"); check(system, false, State::Cooldown);
  }
  // Independent physical cutoff first: reconciliation cannot restart the pump.
  system.begin(); system.update(sample()); system.command("AUTO");
  fakeNow += 5000; assert(pump::poll()); system.update(sample());
  check(system, false, State::Cooldown);
  // Unexpected actuator OFF is also latched rather than silently restarted.
  system.begin(); system.update(sample()); system.command("AUTO");
  pump::command("OFF"); system.service(); check(system, false, State::Locked);
  assert(system.status().decision.reason == watering::Reason::ActuatorStopped);
  // Repeated manual ON on the exact cutoff cannot become a new activation.
  system.begin(); system.update(sample()); system.command("ON");
  fakeNow += 4999; system.update(sample()); fakeNow += 1;
  assert(system.command("ON") == Result::AlreadyOn); check(system, false, State::Cooldown);
  // Mode change from a running manual test stops and locks it.
  system.begin(); system.update(sample()); system.command("ON"); system.command("AUTO");
  check(system, false, State::Locked);
  // Rollover preserves both freshness and the physical cutoff.
  fakeNow = std::numeric_limits<uint32_t>::max() - 1000;
  system.begin(); system.update(sample()); system.command("AUTO");
  fakeNow = 3998; system.update(sample()); check(system, true, State::Watering);
  fakeNow = 3999; system.update(sample()); check(system, false, State::Cooldown);
  // Boot does not retain AUTO or a prior snapshot.
  system.begin(); check(system, false, State::Idle); assert(system.status().mode == Mode::Manual);
  // Production parser completion does not directly call low-level pump ON.
  pump::SerialCommands parser; const char* line = nullptr;
  assert(!parser.feedLine('O', line)); assert(!parser.feedLine('N', line));
  const int beforeLine = writes;
  assert(parser.feedLine('\n', line) && writes == beforeLine);
  assert(system.command(line) == Result::Unsafe);
  system.update(sample());
  assert(system.command("garbage") == Result::Invalid); check(system, false, State::Idle);
  assert(system.command(nullptr) == Result::Invalid);
  std::cout << "PASS: F2.3b mode/safety/pump integration, runtime, rearm, rollover, parser gate\n";
}
