#pragma once
// Deliberately absent from production, including all command handling.
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
#include <Arduino.h>
#include "irrigation.h"
#include <string.h>
namespace guard_test {
class Harness {
 public:
  bool activate(irrigation::System& system) {
    if (system.status().mode != irrigation::Mode::Manual || system.status().pumpOn) return false;
    enabled_ = true;
    return true;
  }
  void disable() { enabled_ = false; }
  irrigation::Result start(irrigation::System& system) {
    if (!enabled_ || starving_) return irrigation::Result::Invalid;
    enabled_ = false;  // One shot, including rejected starts.
    const auto result = system.startManual(5);
    if (result == irrigation::Result::Accepted) {
      started = pump::run();
      starvationAt_ = millis();
      starving_ = true;
    }
    return result;
  }
  bool starving() const { return starving_; }
  bool waiting() const {
    return starving_ && static_cast<uint32_t>(millis() - starvationAt_) < 750;
  }
  void recover(irrigation::System& system) {
    if (!starving_ || waiting()) return;
    // Peek preserves the event for normal System reconciliation. Task context only.
    stopped = pump::guardTestStopEvent();
    system.service();
    recovery = system.status();
    starving_ = false;
  }
  pump::Run started;
  pump::StopEvent stopped;
  irrigation::Status recovery;
 private:
  bool enabled_ = false, starving_ = false;
  uint32_t starvationAt_ = 0;
};
}
#endif
