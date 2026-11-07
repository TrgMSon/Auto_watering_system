#pragma once
#include <stdint.h>
namespace pump {
constexpr uint32_t kMaxRunMs = 5000;  // Legacy and AUTO interval.
constexpr uint32_t kQualificationSeconds = 5;
class QualificationPolicy {
 public:
  QualificationPolicy() = default;
  uint32_t ceilingSeconds() const { return ceiling_; }
#ifndef ARDUINO
  // Host-only injection: unavailable in ESP32 builds.
  static QualificationPolicy forTests(uint32_t seconds) {
    QualificationPolicy p; p.ceiling_ = seconds <= 600 ? seconds : 0; return p;
  }
#endif
 private:
  uint32_t ceiling_ = kQualificationSeconds;
};
struct Run { uint32_t startedMs = 0, durationMs = 0, generation = 0; };
enum class StopCause { None, Timeout, Off, SafetyLeaseExpired };
struct StopEvent { StopCause cause = StopCause::None; uint32_t generation = 0; };
void begin(QualificationPolicy policy = QualificationPolicy{});
bool poll();  // True only when this call expires a run; event remains latched.
bool isOn();
uint32_t remainingMs();
Run run();
StopEvent takeStopEvent();
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
StopEvent guardTestStopEvent();  // Non-consuming diagnostic, no poll/renewal.
#endif
enum class Result { On, Off, Status, AlreadyOn, Invalid, QualificationLimit, GuardUnavailable };
Result start(uint32_t durationSeconds);
// Only System calls this after checking required-data safety. Never starts GPIO.
bool renewSafetyLease(uint32_t sampledAtMs, uint32_t maxReadingAgeMs);
#ifndef ARDUINO
void testGuardEnable(bool enabled);
void testGuardFailure(bool initialization, bool arming);
void testGuardInterrupt();
#endif
Result command(const char* text);  // Legacy low-level compatibility wrapper.
}
