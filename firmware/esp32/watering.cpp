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
void Controller::started(uint32_t now) {
  if (state_ == State::Idle) {
    state_ = State::Watering; startedMs_ = now; automaticRun_ = false;
  }
}
void Controller::interrupted() {
  if (state_ == State::Watering || state_ == State::Cooldown) state_ = State::Locked;
}
bool Controller::runtimeExpired(uint32_t now) const {
  return state_ == State::Watering && static_cast<uint32_t>(now - startedMs_) >= 5000;
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
        else { state_ = State::Watering; startedMs_ = now; automaticRun_ = true; ++attempts_; d.reason = Reason::Dry; }
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
      else { state_ = State::Watering; startedMs_ = now; automaticRun_ = true; ++attempts_; d.reason = Reason::Dry; }
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
    case Reason::ActuatorStopped: return "ACTUATOR_STOPPED";
    case Reason::Cooldown: return "COOLDOWN";
    case Reason::RetryLimit: return "RETRY_LIMIT_RESET_REQUIRED";
    case Reason::Reset: return "RESET_MANUAL";
  }
  return "UNKNOWN";
}
}  // namespace watering
