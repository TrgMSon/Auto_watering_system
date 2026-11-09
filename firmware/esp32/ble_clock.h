#pragma once
#include <stdint.h>
namespace ble_commands {
// Implementation policy requiring review, not a timestamp domain in contract.
// 2024-01-01 through 2099-12-31 UTC, inclusive. No clock-is-already-set check.
constexpr uint32_t kUnixMin = 1704067200u, kUnixMax = 4102444799u;
inline bool validUnix(uint32_t seconds) { return seconds >= kUnixMin && seconds <= kUnixMax; }
struct Clock {
  virtual ~Clock() {}
  // Owner only. True only after setting and confirming UTC readback.
  virtual bool setUtc(uint32_t seconds) = 0;
};
#ifdef ARDUINO
class SystemClock : public Clock {
 public:
  bool setUtc(uint32_t seconds) override;
};
#endif
}
