#pragma once
#include <stdint.h>
#include <esp_mac.h>

namespace device_identity {
inline bool readStationMac(uint8_t (&output)[6]) {
  return esp_read_mac(output, ESP_MAC_WIFI_STA) == ESP_OK;
}
}  // namespace device_identity
