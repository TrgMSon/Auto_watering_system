#pragma once
#include "sensors.h"
#include "ble_auth.h"
namespace plant_ble {
// Starts a separate worker; no radio initialization in the irrigation owner.
bool begin(const ble_commands::KeySource* keys = nullptr);
// Owner-only bounded request; true means queued, worker logs actual window state.
bool requestPairing(bool open,bool manual,bool pumpOn);
// Owner-only, bounded overwrite of latest snapshot, no BLE API or wait.
void publish(const sensors::Reading& reading, bool actuatorOn, uint32_t now);
}
