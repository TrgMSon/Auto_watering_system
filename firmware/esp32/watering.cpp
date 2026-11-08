#include "watering.h"
#include <math.h>

namespace watering {
namespace {
bool percent(float value) { return isfinite(value) && value >= 0 && value <= 100; }
}
Decision Controller::safety(const sensors::Reading& r, uint32_t now) const {
  Decision d;
  d.state = state_;
  d.wateringAttempts = attempts_;
  if (state_ == State::Cooldown) {
    const uint32_t elapsed = static_cast<uint32_t>(now - cooldownStartedMs_);
    d.cooldownRemainingMs = elapsed < 30000 ? 30000 - elapsed : 0;
  }
  const bool configValid = percent(config_.startPercent) && percent(config_.stopPercent) &&
      config_.startPercent < config_.stopPercent && percent(config_.minWaterPercent) &&
      config_.maxReadingAgeMs > 0 && config_.maxReadingAgeMs < 0x80000000u;
  const bool fresh = static_cast<uint32_t>(now - r.sampledAtMs) <= config_.maxReadingAgeMs;
  const bool soilValid = r.soilValid && percent(r.soilPercent);
  const bool calibrationValid = config_.calibrationConfirmed &&
      isfinite(config_.fullDistanceCm) && isfinite(config_.emptyDistanceCm) &&
      config_.fullDistanceCm >= 0 && config_.emptyDistanceCm > config_.fullDistanceCm &&
      isfinite(config_.emptyDistanceCm - config_.fullDistanceCm);
  const bool distanceValid = r.distanceValid && isfinite(r.distanceCm) && r.distanceCm > 0;
  if (calibrationValid && distanceValid && fresh) {
    // Compare endpoints first to avoid overflow with extreme finite distances.
    d.waterPercent = r.distanceCm <= config_.fullDistanceCm ? 100.0f :
        r.distanceCm >= config_.emptyDistanceCm ? 0.0f :
        100.0f * ((config_.emptyDistanceCm - r.distanceCm) /
                  (config_.emptyDistanceCm - config_.fullDistanceCm));
    d.waterValid = true;
  }
  if (!configValid) d.reason = Reason::InvalidConfig;
  else if (!soilValid) d.reason = Reason::SoilInvalid;
  else if (!fresh) d.reason = Reason::Stale;
  else if (!calibrationValid) d.reason = Reason::CalibrationUnknown;
  else if (!distanceValid) d.reason = Reason::DistanceInvalid;
  else if (!d.waterValid || d.waterPercent < config_.minWaterPercent) d.reason = Reason::LowWater;
  return d;
}
void Controller::latchRun(uint32_t now, uint32_t durationMs) {
  startedMs_ = now; durationMs_ = durationMs;
}
void Controller::started(uint32_t now, uint32_t durationMs) {
  if (state_ == State::Idle) {
    state_ = State::Watering; latchRun(now, durationMs); automaticRun_ = false;
  }
}
void Controller::interrupted() {
  if (state_ == State::Watering || state_ == State::Cooldown) state_ = State::Locked;
}
bool Controller::runtimeExpired(uint32_t now) const {
  return state_ == State::Watering && static_cast<uint32_t>(now - startedMs_) >= durationMs_;
}
void Controller::completed(uint32_t now, const sensors::Reading& reading) {
  if (state_ != State::Watering) return;
  const bool safe = safety(reading, now).reason == Reason::Idle;
  if (automaticRun_ && safe && reading.soilPercent >= config_.stopPercent) {
    state_ = State::Idle; attempts_ = 0; return;
  }
  if (automaticRun_ && attempts_ >= 3) {
    state_ = State::Locked;
    retryLocked_ = true;
  } else if (!safe) {
    interrupted();  // A coincident timeout must not hide an unsafe interruption.
  } else {
    state_ = State::Cooldown;
    cooldownStartedMs_ = now;  // Full OFF interval, even when cutoff was serviced late.
  }
}
bool Controller::reset() {
  if (state_ == State::Watering || state_ == State::Cooldown) return false;
  state_ = State::Idle;
  attempts_ = 0;
  retryLocked_ = false;
  return true;
}
Decision Controller::evaluate(const sensors::Reading& r, uint32_t now, bool automatic) {
  Decision d = safety(r, now);
  const bool fresh = static_cast<uint32_t>(now - r.sampledAtMs) <= config_.maxReadingAgeMs;
  const bool soilValid = r.soilValid && percent(r.soilPercent);
  if (runtimeExpired(now)) {
    completed(now, r);
    if (retryLocked_) d.reason = Reason::RetryLimit;
    else if (state_ == State::Idle) d.reason = Reason::Wet;
    else if (state_ == State::Cooldown) d.reason = Reason::Timeout;
  } else if (state_ == State::Locked) {
    // Only a valid, fresh wet soil reading clears a lock. Water may remain unsafe.
    if (!retryLocked_ && d.reason != Reason::InvalidConfig && soilValid && fresh && r.soilPercent >= config_.stopPercent) {
      state_ = State::Idle;
      attempts_ = 0;
      d.reason = Reason::Wet;
    } else {
      d.reason = retryLocked_ ? Reason::RetryLimit : Reason::Locked;
    }
  } else {
    const Reason unsafe = d.reason;
    if (unsafe != Reason::Idle) {
      // Latch interruptions during a run too: no repeated dry-soil retry cycles.
      interrupted();
      d.reason = unsafe;
    } else if (state_ == State::Cooldown) {
      if (d.cooldownRemainingMs != 0) {
        d.reason = Reason::Cooldown;
      } else if (automatic && r.soilPercent < config_.startPercent) {
        if (attempts_ >= 3) { state_ = State::Locked; retryLocked_ = true; d.reason = Reason::RetryLimit; }
        else { state_ = State::Watering; latchRun(now, 5000); automaticRun_ = true; ++attempts_; d.reason = Reason::Dry; }
      } else {
        state_ = State::Idle;
        if (r.soilPercent >= config_.stopPercent) attempts_ = 0;
        d.reason = Reason::Idle;
      }
    } else if (automatic && r.soilPercent >= config_.stopPercent) {
      state_ = State::Idle;
      attempts_ = 0;
      d.reason = Reason::Wet;
    } else if (state_ == State::Watering) {
      d.reason = Reason::Hysteresis;
    } else if (automatic && r.soilPercent < config_.startPercent) {
      if (attempts_ >= 3) { state_ = State::Locked; retryLocked_ = true; d.reason = Reason::RetryLimit; }
      else { state_ = State::Watering; latchRun(now, 5000); automaticRun_ = true; ++attempts_; d.reason = Reason::Dry; }
    }
  }
  d.state = state_;
  d.desiredPumpOn = state_ == State::Watering;
  d.wateringAttempts = attempts_;
  d.cooldownRemainingMs = state_ == State::Cooldown ?
      (static_cast<uint32_t>(now - cooldownStartedMs_) < 30000 ?
       30000 - static_cast<uint32_t>(now - cooldownStartedMs_) : 0) : 0;
  return d;
}
const char* name(State state) {
  switch (state) {
    case State::Idle: return "IDLE";
    case State::Watering: return "WATERING";
    case State::Locked: return "LOCKED";
    case State::Cooldown: return "COOLDOWN";
  }
  return "UNKNOWN";
}
const char* name(Reason reason) {
  switch (reason) {
    case Reason::Dry: return "SOIL_DRY";
    case Reason::Hysteresis: return "HYSTERESIS";
    case Reason::Wet: return "SOIL_WET";
    case Reason::Idle: return "IDLE_BAND";
    case Reason::Timeout: return "TIMEOUT";
    case Reason::Locked: return "LOCKED_UNTIL_WET";
    case Reason::InvalidConfig: return "INVALID_CONFIG";
    case Reason::CalibrationUnknown: return "CALIBRATION_UNKNOWN";
    case Reason::SoilInvalid: return "SOIL_INVALID";
    case Reason::DistanceInvalid: return "DISTANCE_INVALID";
    case Reason::Stale: return "STALE";
    case Reason::LowWater: return "LOW_WATER";
    case Reason::ManualStop: return "MANUAL_STOP";
    case Reason::SafetyLeaseExpired: return "SAFETY_LEASE_EXPIRED";
    case Reason::ActuatorStopped: return "ACTUATOR_STOPPED";
    case Reason::Cooldown: return "COOLDOWN";
    case Reason::RetryLimit: return "RETRY_LIMIT_RESET_REQUIRED";
    case Reason::Reset: return "RESET_MANUAL";
  }
  return "UNKNOWN";
}
}  // namespace watering

namespace watering {
void AutonomousController::stop(uint32_t now) {
  if (state_ != State::Watering) return;
  state_ = State::Cooldown; cooling_ = true; stopped_ = now;
  if (inCycle_ == 2 && cycle_ == 1) { waiting_ = true; retryAt_ = now; }
}
void AutonomousController::setAutomatic(bool enabled, uint32_t now) {
  if (!enabled && automatic_) { stop(now); wet_ = safe_ = false; }
  automatic_ = enabled;
}
void AutonomousController::latchFault(Block fault, uint32_t now) {
  if (fault != Block::ActuatorFault && fault != Block::StorageFault) return;
  stop(now); fault_ = fault; wet_ = safe_ = false; state_ = State::Locked;
}
AutonomousDecision AutonomousController::decision(Block block) const {
  AutonomousDecision d;
  d.state = state_; d.block = block; d.desiredPumpOn = state_ == State::Watering;
  d.automatic = automatic_; d.episodeActive = episode_;
  d.cycle = cycle_; d.reservationsInCycle = inCycle_; d.reservationsTotal = total_;
  return d;
}
AutonomousDecision AutonomousController::evaluate(const sensors::Reading& r, uint32_t now, uint64_t id) {
  return evaluateImpl(r, now, id, false);
}
AutonomousDecision AutonomousController::requestManual(const sensors::Reading& r, uint32_t now, uint64_t id) {
  return evaluateImpl(r, now, id, true);
}
bool AutonomousController::acknowledgeRun(uint32_t started, uint32_t duration) {
  if (state_ != State::Watering || !duration || duration > kRunMs) return false;
  started_ = started; runDuration_ = duration; return true;
}
AutonomousDecision AutonomousController::evaluateImpl(const sensors::Reading& r, uint32_t now, uint64_t id, bool manual) {
  const auto safety = safety_.safety(r, now);
  const bool valid = safety.reason == Reason::Idle;
  const bool fresh = static_cast<uint32_t>(now - r.sampledAtMs) <= config_.maxReadingAgeMs;
  const bool newer = !haveSample_ || id > sampleId_;
  const bool continuous = haveSample_ &&
      static_cast<uint32_t>(r.sampledAtMs - previousSample_) <= config_.maxReadingAgeMs;
  if (newer) { haveSample_ = true; sampleId_ = id; previousSample_ = r.sampledAtMs; }
  // Missing acquisitions break evidence even when the same old sample is replayed.
  if (!valid || !fresh) wet_ = safe_ = false;
  if (fault_ == Block::ActuatorFault || fault_ == Block::StorageFault)
    return decision(fault_);
  // Safety takes precedence over a coincident runtime deadline or budget limit.
  if (!valid) {
    stop(now); state_ = State::Locked;
    fault_ = safety.reason == Reason::LowWater ? Block::WaterUnsafe : Block::SensorUnsafe;
    return decision(fault_);
  }
  if (state_ == State::Watering) {
    if (static_cast<uint32_t>(now - started_) >= runDuration_ || (!manualRun_ && r.soilPercent >= config_.stopPercent))
      stop(now);
  }
  if (newer) {
    if (!continuous) wet_ = safe_ = false;
    if (!safe_) { safe_ = true; safeSince_ = r.sampledAtMs; }
    if (state_ != State::Watering && r.soilPercent >= config_.stopPercent) {
      if (!wet_) { wet_ = true; wetSince_ = r.sampledAtMs; }
    } else wet_ = false;
  }
  if (fault_ == Block::SensorUnsafe || fault_ == Block::WaterUnsafe) {
    if (!newer || !safe_ || static_cast<uint32_t>(r.sampledAtMs - safeSince_) < kStableMs)
      return decision(fault_);
    fault_ = Block::None; state_ = State::Idle;
  }
  // RAM-only initialization: B2 must commit this transition before granting quota.
  if (episode_ && newer && wet_ && state_ != State::Watering &&
      static_cast<uint32_t>(r.sampledAtMs - wetSince_) >= kStableMs) {
    episode_ = false; cycle_ = inCycle_ = total_ = 0; waiting_ = false;
#if WATERING_DEMO_RETRY
    episodeRetryMs_ = kRetryMs;
#endif
    state_ = cooling_ ? State::Cooldown : State::Idle;
    return decision(Block::None); // Persist episode renewal before any MANUAL proposal.
  }
  if (state_ == State::Watering) return decision(Block::None);
  if (cooling_) {
    // A fresh acquisition after the entire OFF interval is mandatory.
    if (static_cast<uint32_t>(now - stopped_) < kCooldownMs || !newer ||
        static_cast<uint32_t>(r.sampledAtMs - stopped_) < kCooldownMs) {
      state_ = State::Cooldown; return decision(automatic_ ? Block::None : Block::ManualInterruption);
    }
    cooling_ = false;
  }
  if (total_ >= 4) { state_ = State::Locked; return decision(Block::BudgetExhausted); }
  if (waiting_) {
#if WATERING_DEMO_RETRY
    const uint32_t kRetryMs = episodeRetryMs_; // Episode-local override only.
#endif
    if (static_cast<uint32_t>(now - retryAt_) < kRetryMs || !newer ||
        static_cast<uint32_t>(r.sampledAtMs - retryAt_) < kRetryMs) {
      state_ = State::Locked; return decision(Block::WaitingRetry);
    }
    // Open cycle 2 only when an actual dry acquisition requests its first run.
  }
  state_ = State::Idle;
  if (!automatic_ && !manual) return decision(Block::ManualInterruption);
  if ((!newer && !manual) || (!manual && r.soilPercent >= config_.startPercent)) return decision(Block::None);
  if (!episode_) { episode_ = true; cycle_ = 1; }
  if (waiting_) { cycle_ = 2; inCycle_ = 0; waiting_ = false; }
  ++inCycle_; ++total_;  // Reserve a full 5s before the domain requests ON.
  state_ = State::Watering; started_ = now; runDuration_ = kRunMs; manualRun_ = manual; wet_ = false;
  return decision(Block::None);
}
}  // namespace watering

namespace watering {
bool AutonomousController::restore(const AutonomousRestore& r, uint32_t now) {
  // Restore resets all sample evidence and mode; never resumes a run.
  *this = AutonomousController(config_);
  const bool counts = r.total <= 4 && (r.total == 0 ? r.cycle == 0 && !r.episode :
      r.episode && r.cycle == (r.total <= 2 ? 1u : 2u));
  if (!r.initialized || !counts || (r.pending && !r.total) ||
      (r.total && !r.pending && !r.cooldown) ||
      (r.waiting != (r.total == 2 && !r.pending))) {
    latchFault(Block::StorageFault, now); return false;
  }
  episode_ = r.episode; cycle_ = r.cycle; total_ = r.total;
  inCycle_ = total_ ? (cycle_ == 1 ? total_ : total_ - 2) : 0;
  cooling_ = r.cooldown; waiting_ = r.waiting;
  stopped_ = retryAt_ = now;
  if (r.pending || r.maintenance) latchFault(Block::StorageFault, now);
  else state_ = cooling_ ? State::Cooldown : total_ >= 4 || waiting_ ? State::Locked : State::Idle;
  return true;
}
}  // namespace watering

namespace watering {
const char* name(Block b) {
  switch(b) {
    case Block::None:return "NONE";
    case Block::BudgetExhausted:return "BUDGET_EXHAUSTED";
    case Block::WaitingRetry:return "WAITING_RETRY";
    case Block::SensorUnsafe:return "SENSOR_UNSAFE";
    case Block::WaterUnsafe:return "WATER_UNSAFE";
    case Block::ManualInterruption:return "MANUAL_INTERRUPTION";
    case Block::ActuatorFault:return "ACTUATOR_FAULT";
    case Block::StorageFault:return "STORAGE_FAULT";
  }
  return "UNKNOWN";
}
}
