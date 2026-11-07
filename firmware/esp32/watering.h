#pragma once
#include "sensors.h"

namespace watering {
enum class State { Idle, Watering, Cooldown, Locked };
enum class Reason {
  Dry, Hysteresis, Wet, Idle, Timeout, Locked,
  InvalidConfig, CalibrationUnknown, SoilInvalid, DistanceInvalid, Stale, LowWater,
  ManualStop, ActuatorStopped, SafetyLeaseExpired, Cooldown, RetryLimit, Reset
};
struct Config {
  float startPercent = 30;
  float stopPercent = 70;
  float minWaterPercent = 20;
  float fullDistanceCm = NAN;
  float emptyDistanceCm = NAN;
  bool calibrationConfirmed = false;
  uint32_t maxReadingAgeMs = 4000;  // Provisional: two acquisition intervals.
};
struct Decision {
  State state = State::Idle;
  bool desiredPumpOn = false;
  bool waterValid = false;
  float waterPercent = NAN;
  Reason reason = Reason::Idle;
  uint32_t cooldownRemainingMs = 0;
  unsigned wateringAttempts = 0;
};
class Controller {
 public:
  explicit Controller(const Config& config = Config{}) : config_(config) {}
  Decision evaluate(const sensors::Reading& reading, uint32_t nowMs, bool automatic = true);
  // The same safety gate is used for manual and automatic activation.
  Decision safety(const sensors::Reading& reading, uint32_t nowMs) const;
  void started(uint32_t nowMs, uint32_t durationMs = 5000);
  void latchRun(uint32_t nowMs, uint32_t durationMs);  // Called after a successful manual activation.
  void interrupted();  // Preserve existing lock; latch an interrupted run.
  bool runtimeExpired(uint32_t nowMs) const;
  void completed(uint32_t nowMs, const sensors::Reading& reading);
  bool reset();  // Never starts a run; cannot shorten a cooldown.
  void configure(const Config& config) { config_ = config; }  // Preserve all runtime state.
  bool retryLimited() const { return retryLocked_; }
  State state() const { return state_; }
 private:
  Config config_;
  State state_ = State::Idle;
  uint32_t startedMs_ = 0;
  uint32_t durationMs_ = 5000;
  uint32_t cooldownStartedMs_ = 0;
  unsigned attempts_ = 0;
  bool automaticRun_ = false;
  bool retryLocked_ = false;
};
const char* name(State state);
const char* name(Reason reason);
}  // namespace watering
