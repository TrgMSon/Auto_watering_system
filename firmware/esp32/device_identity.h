#pragma once
#include <stdint.h>
#include <string.h>

namespace device_identity {
constexpr uint8_t kStationMac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEF, 0xFE};

inline void copyStationMac(uint8_t (&output)[6]) {
  memcpy(output, kStationMac, sizeof(kStationMac));
}
}  // namespace device_identity
