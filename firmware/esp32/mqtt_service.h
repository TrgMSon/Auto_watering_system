#pragma once
#include "telemetry_snapshot.h"
namespace plant_mqtt {
bool begin();
// Owner calls with a value snapshot; one-slot mailbox, no sensor/SDK access.
void publish(const telemetry_snapshot::Snapshot& snapshot);
}
