#pragma once
#include "watering.h"
#include "configuration.h"

namespace irrigation {
enum class Mode { Manual, Auto };
enum class Result { Accepted, AlreadyOn, Unsafe, Locked, ManualOnly, Invalid, Cooldown, ConfigSaved, ConfigUnchanged, ConfigFailed, ConfigBusy };
watering::Config confirmedConfig();
struct Status {
  Mode mode = Mode::Manual;
  bool pumpOn = false;  // Commanded GPIO state, not contact/current feedback.
  uint32_t remainingMs = 0;
  bool soilValid = false;
  watering::Decision decision;
};
class System {
 public:
  explicit System(const watering::Config& config = confirmedConfig())
      : config_(config), controller_(config) {}
  void begin(configuration::Storage* storage = nullptr);
  configuration::Result updateConfiguration(const configuration::Config& candidate);
  const configuration::Manager& configurationStatus() const { return settings_; }
  void update(const sensors::Reading& reading);
  void service();
  Result command(const char* text);
  const Status& status() const { return status_; }
  const sensors::Reading& reading() const { return reading_; }
 private:
  void loadConfiguration(configuration::Storage& storage);
  void stop(watering::Reason reason);
  void snapshot();
  configuration::Manager settings_;
  watering::Config config_;
  watering::Controller controller_;
  sensors::Reading reading_;
  Status status_;
};
const char* name(Mode mode);
const char* name(Result result);
}
