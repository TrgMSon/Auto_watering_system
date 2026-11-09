#pragma once
#include "watering.h"
#include "autonomous_feature.h"
#if WATERING_AUTONOMOUS_TEST
#include "autonomous_owner.h"
#endif
#include "pump.h"
#include "configuration.h"
#include "desired_mode.h"
#include "telemetry_snapshot.h"
#include <atomic>

namespace irrigation {
enum class Mode { Manual, Auto };
enum class RemoteConfigResult { Ok, Invalid, PumpRunning, OtaInProgress, StorageError };
enum class Result { Accepted, AlreadyOn, Unsafe, Locked, ManualOnly, Invalid, Cooldown, ConfigSaved, ConfigUnchanged, ConfigFailed, ConfigBusy, InvalidDuration, ExceedsConfiguredMaximum, QualificationLimit, GuardUnavailable, ModeStorageFailed, OtaInProgress };
watering::Config confirmedConfig();
struct Status {
  Mode mode = Mode::Manual;
  Mode desiredMode = Mode::Manual;
  bool pendingBootAuto = false, modePersisted = false, modeStorageError = false;
  bool pumpOn = false;  // Commanded GPIO state, not contact/current feedback.
  uint32_t remainingMs = 0;
  bool soilValid = false;
  watering::Decision decision;
#if WATERING_AUTONOMOUS_TEST
  watering::Block autonomousBlock = watering::Block::None;
#endif
};
class System {
 public:
  explicit System(const watering::Config& config = confirmedConfig(),
      pump::QualificationPolicy qualification = pump::QualificationPolicy{})
      : config_(config), controller_(config), qualification_(qualification)
#if WATERING_AUTONOMOUS_TEST
      , autonomous_(config)
#endif
      {}
  void begin(configuration::Storage* storage = nullptr, desired_mode::Storage* modes = nullptr
#if WATERING_AUTONOMOUS_TEST
      , autonomous_storage::Ledger* ledger = nullptr
#endif
      );
  void setOtaLock(bool locked) { otaLocked_.store(locked); }
  bool otaLocked() const { return otaLocked_.load(); }
#if WATERING_AUTONOMOUS_TEST
  void setActivationGate(uint64_t (*capture)(), bool (*allowed)(uint64_t), pump::Result (*start)(uint64_t,uint32_t)) { autonomous_.setGate(capture,allowed,start); }
#endif
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
  std::atomic<bool> otaLocked_{false};
#if WATERING_AUTONOMOUS_TEST
  experimental_watering::Owner autonomous_;
  void autonomousSnapshot();
#endif
};
const char* name(Mode mode);
const char* name(Result result);
}
