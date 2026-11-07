#include "irrigation.h"
#include "pump.h"
#include <Arduino.h>
#include <string.h>

namespace irrigation {
watering::Config confirmedConfig() {
  watering::Config config;
  const configuration::Config defaults;
  config.startPercent = defaults.soilStart; config.stopPercent = defaults.soilStop;
  config.minWaterPercent = defaults.waterMin;
  config.fullDistanceCm = defaults.fullCm;
  config.emptyDistanceCm = defaults.emptyCm;  // Lowest tested safe level, not an empty tank.
  config.calibrationConfirmed = true;
  return config;
}
void System::loadConfiguration(configuration::Storage& storage) {
  // Boot-only entry point, after begin() establishes GPIO OFF.
  if (status_.mode != Mode::Manual || pump::isOn()) return;
  settings_.load(&storage);
  const auto& c = settings_.active();
  config_.startPercent=c.soilStart; config_.stopPercent=c.soilStop;
  config_.minWaterPercent=c.waterMin; config_.fullDistanceCm=c.fullCm;
  config_.emptyDistanceCm=c.emptyCm; config_.calibrationConfirmed=true;
  controller_.configure(config_);
  status_.decision = controller_.safety(reading_, millis());
  snapshot();
}
configuration::Result System::updateConfiguration(const configuration::Config& candidate) {
  const auto result = settings_.update(candidate, status_.mode == Mode::Manual,
      !status_.pumpOn && !pump::isOn() && controller_.state() != watering::State::Watering);
  if (result == configuration::Result::Saved || result == configuration::Result::Unchanged) {
    const auto& c = settings_.active();
    config_.startPercent=c.soilStart; config_.stopPercent=c.soilStop;
    config_.minWaterPercent=c.waterMin; config_.fullDistanceCm=c.fullCm;
    config_.emptyDistanceCm=c.emptyCm; config_.calibrationConfirmed=true;
    controller_.configure(config_);
    status_.decision = controller_.safety(reading_, millis());
    snapshot();  // No evaluate/reset: preserve locks, attempts and cooldown.
  }
  return result;
}
void System::begin(configuration::Storage* storage) {
  pump::begin(qualification_);
  controller_ = watering::Controller(config_);
  reading_ = sensors::Reading{};
  status_ = Status{};  // No mode persistence: every boot MANUAL/OFF.
  if (storage) loadConfiguration(*storage);
  service();
}
void System::update(const sensors::Reading& reading) {
  reading_ = reading;
  service();
}
void System::snapshot() {
  status_.remainingMs = pump::remainingMs();
  status_.pumpOn = status_.remainingMs != 0;
  const uint32_t age = static_cast<uint32_t>(millis() - reading_.sampledAtMs);
  status_.soilValid = reading_.soilValid && isfinite(reading_.soilPercent) &&
      reading_.soilPercent >= 0 && reading_.soilPercent <= 100 && age <= config_.maxReadingAgeMs;
}
void System::stop(watering::Reason reason) {
  pump::command("OFF");
  pump::takeStopEvent();  // Explicit stop reconciles this generation here.
  if (reason == watering::Reason::Timeout) controller_.completed(millis(), reading_);
  else if (!(reason == watering::Reason::ManualStop && controller_.state() == watering::State::Cooldown))
    controller_.interrupted();
  // Report fresh safety metadata without initiating a new run or clearing its lock.
  status_.decision = controller_.safety(reading_, millis());
  // No evaluate here: a timeout must be visible before a later wet evaluation unlocks.
  status_.decision.state = controller_.state();
  const auto safetyReason = status_.decision.reason;
  status_.decision.reason = reason;
  if (reason == watering::Reason::Timeout) {
    if (controller_.retryLimited()) status_.decision.reason = watering::Reason::RetryLimit;
    else if (controller_.state() == watering::State::Locked) status_.decision.reason = safetyReason;
    else if (controller_.state() == watering::State::Idle) status_.decision.reason = watering::Reason::Wet;
  }
  status_.decision.desiredPumpOn = false;
  snapshot();
}
void System::service() {
  const bool expectedOn = status_.pumpOn;
  pump::poll();
  const bool actualOn = pump::isOn();
  const auto event = pump::takeStopEvent();
  if (event.cause != pump::StopCause::None || (expectedOn && !actualOn)) {
    stop(event.cause == pump::StopCause::Timeout ? watering::Reason::Timeout :
        event.cause == pump::StopCause::SafetyLeaseExpired ? watering::Reason::SafetyLeaseExpired : watering::Reason::ActuatorStopped);
    return;
  }
  status_.decision = controller_.evaluate(reading_, millis(), status_.mode == Mode::Auto);
  if (!status_.decision.desiredPumpOn && actualOn) {
    pump::command("OFF");
    pump::takeStopEvent();  // Controller already processed this safety/wet stop.
  } else if (status_.mode == Mode::Auto && status_.decision.desiredPumpOn && !actualOn) {
    // Exactly one activation on an IDLE -> WATERING transition, never a timer reset.
    if (pump::start(5) != pump::Result::On) {
      stop(watering::Reason::ActuatorStopped);
      return;
    }
    const auto run = pump::run();
    controller_.latchRun(run.startedMs, run.durationMs);
  }
  if (controller_.state() == watering::State::Watering &&
      controller_.safety(reading_, millis()).reason == watering::Reason::Idle) {
    const uint32_t age = static_cast<uint32_t>(millis() - reading_.sampledAtMs);
    if (age <= config_.maxReadingAgeMs)
      pump::renewSafetyLease(reading_.sampledAtMs, config_.maxReadingAgeMs);
  }
  snapshot();
  // remainingMs() also enforces the independent cutoff, including boundary races.
  if (status_.decision.state == watering::State::Watering && !status_.pumpOn) {
    const auto stopped = pump::takeStopEvent();
    stop(stopped.cause == pump::StopCause::SafetyLeaseExpired ?
        watering::Reason::SafetyLeaseExpired : watering::Reason::Timeout);
  }
}
Result System::command(const char* text) {
  if (text && strcmp(text, "ON") == 0) return startManual(5);
  if (text && strcmp(text, "CONFIG") == 0) return Result::Accepted;
  if (text && (strncmp(text, "SET ", 4) == 0 || strcmp(text, "RESET_CONFIG") == 0)) {
    configuration::Config candidate;
    if (strcmp(text, "RESET_CONFIG") != 0 &&
        !configuration::parseSet(text, settings_.active(), candidate)) return Result::Invalid;
    switch (updateConfiguration(candidate)) {
      case configuration::Result::Saved: return Result::ConfigSaved;
      case configuration::Result::Unchanged: return Result::ConfigUnchanged;
      case configuration::Result::Busy: return Result::ConfigBusy;
      case configuration::Result::StorageFailed: return Result::ConfigFailed;
      case configuration::Result::Invalid: return Result::Invalid;
    }
  }
  // Reset cannot execute AUTO or shorten an active interval/cooldown.
  if (text && strcmp(text, "RESET_WATERING") == 0) {
    if (controller_.state() == watering::State::Watering) return Result::AlreadyOn;
    if (controller_.state() == watering::State::Cooldown) return Result::Cooldown;
    if (controller_.safety(reading_, millis()).reason != watering::Reason::Idle) return Result::Unsafe;
    status_.mode = Mode::Manual;
    pump::command("OFF");
    controller_.reset();
    status_.decision = controller_.safety(reading_, millis());
    status_.decision.reason = watering::Reason::Reset;
    snapshot();
    return Result::Accepted;
  }
  // Emergency commands bypass service/start and never wait for a new acquisition.
  if (text && (strcmp(text, "OFF") == 0 || strcmp(text, "MANUAL") == 0)) {
    return stopManual();
  }
  service();
  if (!text) return Result::Invalid;
  if (strcmp(text, "STATUS") == 0) return Result::Accepted;
  if (strcmp(text, "AUTO") == 0) {
    if (status_.mode != Mode::Auto) {
      // Mode changes cannot transfer an active manual run into AUTO.
      if (status_.pumpOn) stop(watering::Reason::ManualStop);
      status_.mode = Mode::Auto;
    }
    service();
    return Result::Accepted;
  }
  return Result::Invalid;
}
Result System::stopManual() {
  status_.mode = Mode::Manual;
  stop(watering::Reason::ManualStop);
  return Result::Accepted;
}
Result System::startManual(uint32_t durationSeconds) {
  const bool wasRunning = status_.pumpOn || pump::isOn();
  service();
  if (durationSeconds < 1 || durationSeconds > 600) return Result::InvalidDuration;
  if (status_.mode != Mode::Manual) return Result::ManualOnly;
  if (wasRunning) return Result::AlreadyOn;
  if (durationSeconds > settings_.active().maxRunSeconds) return Result::ExceedsConfiguredMaximum;
  if (durationSeconds > qualification_.ceilingSeconds()) return Result::QualificationLimit;
  if (status_.decision.state == watering::State::Locked) return Result::Locked;
  if (status_.decision.state == watering::State::Cooldown) return Result::Cooldown;
  if (controller_.safety(reading_, millis()).reason != watering::Reason::Idle) return Result::Unsafe;
  const auto result = pump::start(durationSeconds);
  if (result == pump::Result::GuardUnavailable) return Result::GuardUnavailable;
  if (result != pump::Result::On) return result == pump::Result::QualificationLimit ?
      Result::QualificationLimit : Result::AlreadyOn;
  const auto run = pump::run();
  controller_.started(run.startedMs, run.durationMs);
  service();
  return status_.pumpOn ? Result::Accepted : Result::Unsafe;
}
const char* name(Mode mode) { return mode == Mode::Auto ? "AUTO" : "MANUAL"; }
const char* name(Result result) {
  switch (result) {
    case Result::GuardUnavailable: return "GUARD_UNAVAILABLE";
    case Result::InvalidDuration: return "INVALID_DURATION";
    case Result::ExceedsConfiguredMaximum: return "EXCEEDS_CONFIGURED_MAXIMUM";
    case Result::QualificationLimit: return "QUALIFICATION_LIMIT";
    case Result::ConfigSaved: return "CONFIG_SAVED";
    case Result::ConfigUnchanged: return "CONFIG_UNCHANGED_PERSISTED";
    case Result::ConfigFailed: return "CONFIG_SAVE_FAILED";
    case Result::ConfigBusy: return "CONFIG_REJECTED_MANUAL_OFF_REQUIRED";
    case Result::Accepted: return "ACCEPTED";
    case Result::AlreadyOn: return "ALREADY_ON_NO_EXTENSION";
    case Result::Unsafe: return "REJECTED_UNSAFE";
    case Result::Locked: return "REJECTED_LOCKED";
    case Result::ManualOnly: return "REJECTED_MANUAL_ONLY";
    case Result::Invalid: return "INVALID_COMMAND";
    case Result::Cooldown: return "REJECTED_COOLDOWN";
  }
  return "UNKNOWN";
}
}
