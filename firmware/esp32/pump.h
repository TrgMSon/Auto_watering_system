#pragma once
#include <stdint.h>

namespace pump {
constexpr uint32_t kMaxRunMs = 5000;
void begin();
// Service before commands and around sensor acquisition.
bool poll();  // True when the timeout switched the output OFF.
bool isOn();
uint32_t remainingMs();
enum class Result { On, Off, Status, AlreadyOn, Invalid };
Result command(const char* text);
}  // namespace pump
