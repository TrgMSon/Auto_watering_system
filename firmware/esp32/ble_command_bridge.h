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
// Called exclusively by the existing irrigation owner. No BLE API or wait.
void serviceOwner(irrigation::System& system, ble_commands::Clock* clock = nullptr);
}
