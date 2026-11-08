#pragma once
#include "sensors.h"
#include "autonomous_feature.h"

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

// B1 domain prototype only: RAM accounting, never connected to the pump owner.
namespace watering {
enum class Block { None, BudgetExhausted, WaitingRetry, SensorUnsafe, WaterUnsafe,
                   ManualInterruption, ActuatorFault, StorageFault };
const char* name(Block);
struct AutonomousDecision {
  State state = State::Idle;
  Block block = Block::None;
  bool desiredPumpOn = false;
  bool automatic = false;
  bool episodeActive = false;
  unsigned cycle = 0, reservationsInCycle = 0, reservationsTotal = 0;
};
// Boot-only snapshot from the authoritative persistence component. No old millis.
struct AutonomousRestore {
  bool initialized = false, episode = false, pending = false, maintenance = false;
  bool cooldown = false, waiting = false;
  unsigned cycle = 0, total = 0;
};
class AutonomousController {
 public:
  explicit AutonomousController(const Config& config) : safety_(config), config_(config) {}
  // Explicit mode requests only. OFF stops the domain run and never clears quota.
  void setAutomatic(bool enabled, uint32_t now);
  bool restore(const AutonomousRestore&, uint32_t now);
  AutonomousDecision requestManual(const sensors::Reading&, uint32_t now, uint64_t sampleId);
  void interrupt(uint32_t now) { stop(now); automatic_ = false; wet_ = safe_ = false; }
  void finishRun(uint32_t now) { stop(now); }
  bool acknowledgeRun(uint32_t started, uint32_t durationMs);
#if WATERING_DEMO_RETRY
  // Owner calls only after a successful durable ledger reservation.
  void adoptCommittedRetry(uint32_t retryMs) {
    episodeRetryMs_ = retryMs == 60000 ? 60000 : kRetryMs;
  }
#endif
  uint32_t cooldownRemaining(uint32_t now) const {
    const uint32_t elapsed=now-stopped_;
    return cooling_ && elapsed<kCooldownMs ? kCooldownMs-elapsed : 0;
  }
  void latchFault(Block fault, uint32_t now);
  void configure(const Config& config) { config_ = config; safety_.configure(config); wet_ = safe_ = false; }
  // sampleId must increase for each acquisition, independently of millis rollover.
  AutonomousDecision evaluate(const sensors::Reading&, uint32_t now, uint64_t sampleId);
 private:
  static constexpr uint32_t kRunMs = 5000, kCooldownMs = 30000,
                            kRetryMs = 1800000, kStableMs = 30000;
#if WATERING_DEMO_RETRY
  uint32_t episodeRetryMs_ = kRetryMs; // Restored episodes always use production.
#endif
  Controller safety_;
  Config config_;
  State state_ = State::Idle;
  Block fault_ = Block::None;
  bool automatic_ = false, episode_ = false, cooling_ = false, waiting_ = false;
  unsigned cycle_ = 0, inCycle_ = 0, total_ = 0;
  uint32_t started_ = 0, stopped_ = 0, retryAt_ = 0, runDuration_ = kRunMs;
  bool haveSample_ = false, wet_ = false, safe_ = false, manualRun_ = false;
  uint64_t sampleId_ = 0;
  uint32_t previousSample_ = 0, wetSince_ = 0, safeSince_ = 0;
  AutonomousDecision evaluateImpl(const sensors::Reading&, uint32_t, uint64_t, bool manual);
  void stop(uint32_t now);
  AutonomousDecision decision(Block block) const;
};
}  // namespace watering
