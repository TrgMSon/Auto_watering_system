#pragma once
#include "ble_dispatch.h"
namespace ble_command_bridge {
uint64_t connect(uint16_t connection);
void disconnect(uint16_t connection);
void disable();
uint64_t generation(uint16_t connection);
bool current(uint64_t generation);
bool reserve(uint64_t generation, uint32_t transaction, const ble_commands::Command& command,
    ble_commands::Code code, uint32_t now, uint64_t ingressEpoch = UINT64_MAX);
bool result(ble_commands::Dispatch::Slot& slot);
void release(uint32_t ticket);
void expire(uint32_t now);
// Read-only activation fence: no wire contract change.
bool activationAllowed(uint64_t epoch);
#if WATERING_AUTONOMOUS_TEST
// Owner-only, after a verified reservation. Final OFF fence and GPIO admission
// share the dispatch lock; no NVS or blocking work is performed under this lock.
pump::Result startReserved(uint64_t epoch, uint32_t seconds);
#endif
// Called exclusively by the existing irrigation owner. No BLE API or wait.
void serviceOwner(irrigation::System& system, ble_commands::Clock* clock = nullptr);
}
