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
#if WATERING_AUTONOMOUS_TEST
  autonomous_.configure(config_);
#endif
  status_.decision = controller_.safety(reading_, millis());
  snapshot();
}
configuration::Result System::updateConfiguration(const configuration::Config& candidate) {
  if (otaLocked()) return configuration::Result::Busy;
  // A pending restored AUTO is not a manual configuration session. Explicit
  // MANUAL (or the MQTT safe intermediate) must cancel it before a SET/save.
  if (status_.pendingBootAuto) return configuration::Result::Busy;
  if (status_.mode != Mode::Manual || status_.pumpOn || pump::isOn() ||
      controller_.state() == watering::State::Watering) return configuration::Result::Busy;
  if (!configuration::valid(candidate)) return configuration::Result::Invalid;
  // Serial SET/RESET_CONFIG and typed owner callers need the same safe baseline
  // after an earlier OFF persistence failure, not just MQTT CONFIG.
  if ((!modes_.confirmed() || modes_.automatic()) && !changeDesiredMode(Mode::Manual))
    return configuration::Result::StorageFailed;
  const auto result = settings_.update(candidate, status_.mode == Mode::Manual,
      !status_.pumpOn && !pump::isOn() && controller_.state() != watering::State::Watering);
  if (result == configuration::Result::Saved || result == configuration::Result::Unchanged) {
    const auto& c = settings_.active();
    config_.startPercent=c.soilStart; config_.stopPercent=c.soilStop;
    config_.minWaterPercent=c.waterMin; config_.fullDistanceCm=c.fullCm;
    config_.emptyDistanceCm=c.emptyCm; config_.calibrationConfirmed=true;
    controller_.configure(config_);
#if WATERING_AUTONOMOUS_TEST
  autonomous_.configure(config_);
#endif
    status_.decision = controller_.safety(reading_, millis());
    snapshot();  // No evaluate/reset: preserve locks, attempts and cooldown.
  }
  return result;
}
RemoteConfigResult System::applyRemoteConfiguration(const configuration::Config& candidate, bool automatic) {
  if (otaLocked()) return RemoteConfigResult::OtaInProgress;
  // Owner only. Validate before any mutation, including a temporary mode change.
  if (!configuration::valid(candidate)) return RemoteConfigResult::Invalid;
  if (status_.pumpOn || pump::isOn() || controller_.state() == watering::State::Watering)
    return RemoteConfigResult::PumpRunning;
  // Identical persisted config + mode needs no intermediate flash write.
  const bool unchanged = settings_.persistedSchema() == 2 &&
      configuration::equal(candidate, settings_.active());
  if (unchanged && modes_.confirmed() && modes_.automatic() == automatic) {
    // A boot-pending AUTO still requires a fresh acquisition, even for a no-op.
    service();
    return RemoteConfigResult::Ok;
  }
  // Confirm a safe persisted intermediate before touching the config key.
  // No reset: cooldown, locks and attempts remain intact.
  if (!changeDesiredMode(Mode::Manual)) return RemoteConfigResult::StorageError;
  const bool calibrationConfirmed = config_.calibrationConfirmed;
  const auto saved = updateConfiguration(candidate);
  if (saved != configuration::Result::Saved && saved != configuration::Result::Unchanged) {
    // Partial failure intentionally stays MANUAL/OFF; never restore old AUTO.
    return RemoteConfigResult::StorageError;
  }
  // Remote settings do not confirm a calibration; retain the owner's evidence.
  config_.calibrationConfirmed = calibrationConfirmed;
  controller_.configure(config_);
#if WATERING_AUTONOMOUS_TEST
  autonomous_.configure(config_);
#endif
  if (automatic && !changeDesiredMode(Mode::Auto)) return RemoteConfigResult::StorageError;
  service();  // AUTO may start only through the existing controller and guard.
  return RemoteConfigResult::Ok;
}
bool System::changeDesiredMode(Mode requested) {
  // Owner only. OFF and boot-pending cancellation always precede blocking NVS.
  const bool confirmedAuto = modes_.confirmed() && modes_.automatic();
  if (requested == Mode::Manual || status_.mode != Mode::Auto || !confirmedAuto) {
    status_.mode = Mode::Manual;
    status_.pendingBootAuto = false;
    stop(watering::Reason::ManualStop);
  }
  status_.desiredMode = requested;
  const bool confirmed = modes_.save(requested == Mode::Auto);
  status_.modePersisted = confirmed;
  status_.modeStorageError = !confirmed;
  if (!confirmed) {
    // A write may have reached flash, but runtime must not re-enter AUTO.
    status_.mode = Mode::Manual; status_.pendingBootAuto = false;
    return false;
  }
  status_.mode = requested;
  status_.pendingBootAuto = false;
  return true;
}
void System::begin(configuration::Storage* storage, desired_mode::Storage* modes
#if WATERING_AUTONOMOUS_TEST
    , autonomous_storage::Ledger* ledger
#endif
    ) {
  otaLocked_.store(false);
  pump::begin(qualification_);
  bootMs_ = millis();
  controller_ = watering::Controller(config_);
  reading_ = sensors::Reading{};
  status_ = Status{};  // Applied MANUAL/OFF until a boot-local safe acquisition.
  settings_.load(nullptr);  // Do not retain a previous begin() storage pointer.
  if (storage) loadConfiguration(*storage);
  modes_.load(modes);
  status_.modePersisted = modes_.confirmed();
  status_.desiredMode = modes_.automatic() ? Mode::Auto : Mode::Manual;
  status_.pendingBootAuto = modes_.automatic();
#if WATERING_AUTONOMOUS_TEST
  autonomous_.begin(ledger,bootMs_);
#endif
  service();
}
void System::update(const sensors::Reading& reading) {
  reading_ = reading;
#if WATERING_AUTONOMOUS_TEST
  autonomous_.observe(reading);
#endif
  // update() is called only for a new sensors::poll acquisition in production.
  // Reject old timestamps as well as stale/invalid/low-water/calibration data.
  if (status_.pendingBootAuto && int32_t(reading.sampledAtMs - bootMs_) >= 0 &&
      controller_.safety(reading_, millis()).reason == watering::Reason::Idle) {
    status_.pendingBootAuto = false;
    status_.mode = Mode::Auto;
  }
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
#if WATERING_AUTONOMOUS_TEST
  (void)reason;autonomous_.interrupt();autonomousSnapshot();return;
#endif
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
#if WATERING_AUTONOMOUS_TEST
  if (otaLocked()) { pump::command("OFF"); autonomous_.interrupt(); autonomousSnapshot(); return; }
  autonomous_.service(status_.mode==Mode::Auto);autonomousSnapshot();return;
#endif
  const bool expectedOn = status_.pumpOn;
  pump::poll();
  const bool actualOn = pump::isOn();
  const auto event = pump::takeStopEvent();
  if (event.cause != pump::StopCause::None || (expectedOn && !actualOn)) {
    stop(event.cause == pump::StopCause::Timeout ? watering::Reason::Timeout :
        event.cause == pump::StopCause::SafetyLeaseExpired ? watering::Reason::SafetyLeaseExpired : watering::Reason::ActuatorStopped);
    return;
  }
  if (otaLocked()) {
    if (actualOn) {
      stop(watering::Reason::ManualStop);
      return;
    }
    snapshot();
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
  if (otaLocked() && text && strcmp(text, "OFF") != 0 &&
      strcmp(text, "MANUAL") != 0 && strcmp(text, "STATUS") != 0) return Result::OtaInProgress;
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
#if WATERING_AUTONOMOUS_TEST
    if(pump::isOn())return Result::AlreadyOn;
    if(status_.decision.state==watering::State::Cooldown)return Result::Cooldown;
    if(controller_.safety(reading_,millis()).reason!=watering::Reason::Idle)return Result::Unsafe;
    if(!changeDesiredMode(Mode::Manual))return Result::ModeStorageFailed;
    if(!autonomous_.maintenance())return Result::ConfigFailed;
    autonomousSnapshot();return Result::Accepted;
#endif
    if (controller_.state() == watering::State::Watering) return Result::AlreadyOn;
    if (controller_.state() == watering::State::Cooldown) return Result::Cooldown;
    if (controller_.safety(reading_, millis()).reason != watering::Reason::Idle) return Result::Unsafe;
    const bool persisted = changeDesiredMode(Mode::Manual);
    controller_.reset();
    status_.decision = controller_.safety(reading_, millis());
    status_.decision.reason = watering::Reason::Reset;
    snapshot();
    return persisted ? Result::Accepted : Result::ModeStorageFailed;
  }
  // Emergency commands bypass service/start and never wait for a new acquisition.
  if (text && (strcmp(text, "OFF") == 0 || strcmp(text, "MANUAL") == 0)) {
    return stopManual();
  }
  if (text && strcmp(text, "AUTO") == 0) {
    // Persist before service can start a new AUTO run; no NVS after GPIO ON.
    if (!changeDesiredMode(Mode::Auto)) return Result::ModeStorageFailed;
    service();
    return Result::Accepted;
  }
  service();
  if (!text) return Result::Invalid;
  if (strcmp(text, "STATUS") == 0) return Result::Accepted;
  return Result::Invalid;
}
Result System::stopManual() {
  if (otaLocked()) {
    stop(watering::Reason::ManualStop);
    return Result::Accepted;
  }
  changeDesiredMode(Mode::Manual);
  // GPIO OFF succeeded even if desired-mode persistence is unconfirmed. The
  // public diagnostic carries that distinction without changing BLE codes.
  return Result::Accepted;
}
Result System::startManual(uint32_t durationSeconds) {
  if (otaLocked()) return Result::OtaInProgress;
#if WATERING_AUTONOMOUS_TEST
  if(durationSeconds<1||durationSeconds>600)return Result::InvalidDuration;
  if(status_.mode!=Mode::Manual)return Result::ManualOnly;
  if(durationSeconds>5)return Result::QualificationLimit;
  const auto experimentalResult=autonomous_.manual(durationSeconds);autonomousSnapshot();
  switch(experimentalResult) {
    case experimental_watering::Outcome::Started:return Result::Accepted;
    case experimental_watering::Outcome::Busy:return Result::AlreadyOn;
    case experimental_watering::Outcome::Unsafe:return Result::Unsafe;
    case experimental_watering::Outcome::Cooldown:return Result::Cooldown;
    case experimental_watering::Outcome::ActuatorFault:return Result::GuardUnavailable;
    default:return Result::Locked;
  }
#endif
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
telemetry_snapshot::Snapshot System::telemetrySnapshot(uint32_t now) const {
  telemetry_snapshot::Snapshot snapshot;
  snapshot.reading = reading_;
  // Reuse exactly the active controller's calibration and conversion, including
  // invalid/unconfirmed handling. LOW_WATER is not an invalid sensor reading.
  const auto decision = controller_.safety(reading_, now);
  snapshot.waterPercent = decision.waterPercent;
  snapshot.waterValid = decision.waterValid;
  snapshot.actuatorOn = pump::isOn();
  snapshot.automaticMode = status_.mode == Mode::Auto;
  snapshot.publishedAtMs = now;
  return snapshot;
}
const char* name(Mode mode) { return mode == Mode::Auto ? "AUTO" : "MANUAL"; }
const char* name(Result result) {
  switch (result) {
    case Result::ModeStorageFailed: return "MODE_SAVE_FAILED_RUNTIME_MANUAL_OFF";
    case Result::OtaInProgress: return "OTA_IN_PROGRESS";
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

#if WATERING_AUTONOMOUS_TEST
namespace irrigation {
void System::autonomousSnapshot() {
  // Keep accepted OFF intent even if transport disconnect/expiry discarded its ACK slot.
  if(autonomous_.takeManualRequest() && status_.mode!=Mode::Manual)
    changeDesiredMode(Mode::Manual);
  status_.decision=controller_.safety(reading_,millis());
  const auto& a=autonomous_.status();status_.decision.state=a.state;
  status_.autonomousBlock=a.block;status_.decision.desiredPumpOn=a.desiredPumpOn;
  status_.decision.wateringAttempts=a.reservationsTotal;
  status_.decision.cooldownRemainingMs=autonomous_.cooldownRemaining(millis());
  switch(a.block) {
    case watering::Block::BudgetExhausted:status_.decision.reason=watering::Reason::RetryLimit;break;
    case watering::Block::WaitingRetry:status_.decision.reason=watering::Reason::Cooldown;break;
    case watering::Block::ManualInterruption:status_.decision.reason=watering::Reason::ManualStop;break;
    case watering::Block::ActuatorFault:status_.decision.reason=watering::Reason::ActuatorStopped;break;
    case watering::Block::StorageFault:status_.decision.reason=watering::Reason::Locked;break;
    default:if(a.state==watering::State::Cooldown)status_.decision.reason=watering::Reason::Cooldown;break;
  }
  snapshot();
}
}
#endif
