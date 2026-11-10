#pragma once

// Shared compile-time Wi-Fi mode for the runtime and BLE GATT profile.
// wifi_local.h is developer-local and ignored by Git.
#if defined(__has_include)
#if __has_include("wifi_local.h")
#include "wifi_local.h"
#endif
#endif

#ifndef WIFI_FIXED_ENABLED
#define WIFI_FIXED_ENABLED 0
#endif
#ifndef WIFI_FIXED_SSID
#define WIFI_FIXED_SSID ""
#endif
#ifndef WIFI_FIXED_PASSWORD
#define WIFI_FIXED_PASSWORD ""
#endif

#if WIFI_FIXED_ENABLED != 0 && WIFI_FIXED_ENABLED != 1
#error WIFI_FIXED_ENABLED must be 0 or 1
#endif
