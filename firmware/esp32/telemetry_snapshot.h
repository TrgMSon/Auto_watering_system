#pragma once
#include "sensors.h"
namespace telemetry_snapshot {
// Owner captures the reading and its active-calibration result together. This
// is internal mailbox data, not a new wire field or a mutable System reference.
struct Snapshot {
  sensors::Reading reading;
  float waterPercent = NAN;
  bool waterValid = false;
  bool actuatorOn = false;
  bool automaticMode = false;
  uint32_t publishedAtMs = 0; // Publication never refreshes reading.sampledAtMs.
};
}
