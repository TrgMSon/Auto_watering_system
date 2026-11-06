#include "pump.h"
#include <Arduino.h>
#include <string.h>
#include "board_config.h"

namespace pump {
namespace {
bool running = false;
uint32_t startedMs = 0;
void stop() {
  digitalWrite(board_config::kRelayIn1Pin, LOW);
  running = false;
}
}
void begin() {
  // Verified transistor-stage polarity: test_relay_1.ino (HIGH ON, LOW OFF).
  stop();  // Preload OFF before enabling the output driver.
  pinMode(board_config::kRelayIn1Pin, OUTPUT);
}
bool poll() {
  if (running && static_cast<uint32_t>(millis() - startedMs) >= kMaxRunMs) {
    stop();
    return true;
  }
  return false;
}
bool isOn() { poll(); return running; }
uint32_t remainingMs() {
  const uint32_t elapsed = static_cast<uint32_t>(millis() - startedMs);
  if (running && elapsed >= kMaxRunMs) stop();
  return running ? kMaxRunMs - elapsed : 0;
}
Result command(const char* text) {
  // A repeated ON at the deadline must not restart the just-expired run.
  const bool wasRunning = running;
  poll();
  if (text == nullptr) return Result::Invalid;
  if (strcmp(text, "OFF") == 0) { stop(); return Result::Off; }
  if (strcmp(text, "STATUS") == 0) return Result::Status;
  if (strcmp(text, "ON") != 0) return Result::Invalid;
  if (wasRunning) return Result::AlreadyOn;
  startedMs = millis();
  digitalWrite(board_config::kRelayIn1Pin, HIGH);
  running = true;
  return Result::On;
}
}  // namespace pump
