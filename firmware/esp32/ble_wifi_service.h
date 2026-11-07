#pragma once
#include "ble_wifi_handoff.h"
#include "wifi_runtime.h"
namespace ble_wifi_service {
bool begin(); // One worker owns boot/reconnect and provisioning radio/NVS.
wifi_runtime::Snapshot networkSnapshot(); // synchronized value copy
bool available();
void connection(uint64_t generation);
bool reserve(const ble_commands::WifiAuthentication& auth,uint32_t now);
bool result(uint32_t now,uint64_t& generation,ble_commands::Code& code);
void release(uint64_t generation);
}
