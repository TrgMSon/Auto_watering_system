#pragma once
#include "watering.h"
#include "pump.h"
#include "configuration.h"
#include "desired_mode.h"
#include "telemetry_snapshot.h"

namespace irrigation {
enum class Mode { Manual, Auto };
enum class RemoteConfigResult { Ok, Invalid, PumpRunning, StorageError };
enum class Result { Accepted, AlreadyOn, Unsafe, Locked, ManualOnly, Invalid, Cooldown, ConfigSaved, ConfigUnchanged, ConfigFailed, ConfigBusy, InvalidDuration, ExceedsConfiguredMaximum, QualificationLimit, GuardUnavailable, ModeStorageFailed };
watering::Config confirmedConfig();
struct Status {
  Mode mode = Mode::Manual;
  Mode desiredMode = Mode::Manual;
  bool pendingBootAuto = false, modePersisted = false, modeStorageError = false;
  bool pumpOn = false;  // Commanded GPIO state, not contact/current feedback.
  uint32_t remainingMs = 0;
  bool soilValid = false;
  watering::Decision decision;
};
class System {
 public:
  explicit System(const watering::Config& config = confirmedConfig(),
      pump::QualificationPolicy qualification = pump::QualificationPolicy{})
      : config_(config), controller_(config), qualification_(qualification) {}
  void begin(configuration::Storage* storage = nullptr, desired_mode::Storage* modes = nullptr);
  configuration::Result updateConfiguration(const configuration::Config& candidate);
  RemoteConfigResult applyRemoteConfiguration(const configuration::Config& candidate, bool automatic);
  const configuration::Manager& configurationStatus() const { return settings_; }
  void update(const sensors::Reading& reading);
  void service();
  Result command(const char* text);
  Result startManual(uint32_t durationSeconds);
  Result stopManual();  // Same immediate OFF/MANUAL semantics as Serial.
  const Status& status() const { return status_; }
  const sensors::Reading& reading() const { return reading_; }
  telemetry_snapshot::Snapshot telemetrySnapshot(uint32_t now) const; // Owner-only value capture.
 private:
  void loadConfiguration(configuration::Storage& storage);
  void stop(watering::Reason reason);
  void snapshot();
  bool changeDesiredMode(Mode mode);
  configuration::Manager settings_;
  desired_mode::Manager modes_;
  uint32_t bootMs_ = 0;
  watering::Config config_;
  watering::Controller controller_;
  sensors::Reading reading_;
  Status status_;
  pump::QualificationPolicy qualification_;
};
const char* name(Mode mode);
const char* name(Result result);
}
