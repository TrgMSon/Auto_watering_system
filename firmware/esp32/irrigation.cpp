#include "irrigation.h"
#include "pump.h"
#include <Arduino.h>
#include <string.h>

namespace irrigation {
watering::Config confirmedConfig() {
  watering::Config config;
  config.fullDistanceCm = 3.00f;
  config.emptyDistanceCm = 12.26f;  // Lowest tested safe level, not an empty tank.
  config.calibrationConfirmed = true;
  return config;
}
void System::begin() {
  pump::begin();
  controller_ = watering::Controller(config_);
  reading_ = sensors::Reading{};
  status_ = Status{};  // No mode persistence: every boot MANUAL/OFF.
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
  const bool timeout = pump::poll();
  const bool actualOn = pump::isOn();
  if (timeout || (expectedOn && !actualOn)) {
    stop(timeout || controller_.runtimeExpired(millis()) ? watering::Reason::Timeout : watering::Reason::ActuatorStopped);
    return;
  }
  status_.decision = controller_.evaluate(reading_, millis(), status_.mode == Mode::Auto);
  if (!status_.decision.desiredPumpOn && actualOn) {
    pump::command("OFF");
  } else if (status_.mode == Mode::Auto && status_.decision.desiredPumpOn && !actualOn) {
    // Exactly one activation on an IDLE -> WATERING transition, never a timer reset.
    if (pump::command("ON") != pump::Result::On) {
      stop(watering::Reason::ActuatorStopped);
      return;
    }
  }
  snapshot();
  // remainingMs() also enforces the independent cutoff, including boundary races.
  if (status_.decision.state == watering::State::Watering && !status_.pumpOn) {
    stop(watering::Reason::Timeout);
  }
}
Result System::command(const char* text) {
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
    status_.mode = Mode::Manual;
    stop(watering::Reason::ManualStop);
    return Result::Accepted;
  }
  const bool wasRunning = status_.pumpOn;
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
  if (strcmp(text, "ON") != 0) return Result::Invalid;
  if (status_.mode != Mode::Manual) return Result::ManualOnly;
  if (wasRunning) return Result::AlreadyOn;
  if (status_.decision.state == watering::State::Locked) return Result::Locked;
  if (status_.decision.state == watering::State::Cooldown) return Result::Cooldown;
  const auto safety = controller_.safety(reading_, millis());
  if (safety.reason != watering::Reason::Idle) return Result::Unsafe;
  if (pump::command("ON") != pump::Result::On) return Result::AlreadyOn;
  controller_.started(millis());
  // Recheck immediately: if freshness expires across activation, switch OFF here.
  service();
  return status_.pumpOn ? Result::Accepted : Result::Unsafe;
}
const char* name(Mode mode) { return mode == Mode::Auto ? "AUTO" : "MANUAL"; }
const char* name(Result result) {
  switch (result) {
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
