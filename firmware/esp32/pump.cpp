#include "pump.h"
#include <Arduino.h>
#include <string.h>
#include "board_config.h"
#ifdef ARDUINO
#include "driver/gptimer.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_timer.h"
#define PUMP_IRAM IRAM_ATTR
#else
#define PUMP_IRAM
#endif
namespace pump {
namespace {
constexpr uint32_t kLeaseMs = 250;
bool running = false, guardReady = false;
Run activeRun;
StopEvent stopEvent;
QualificationPolicy qualification;
uint32_t leaseStartedMs = 0, leaseDurationMs = 0;
#ifdef ARDUINO
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
gptimer_handle_t timer = nullptr;
struct Lock { Lock() { portENTER_CRITICAL(&mux); } ~Lock() { portEXIT_CRITICAL(&mux); } };
uint32_t PUMP_IRAM nowMs() { return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL); }
#else
bool dispatchGuard = false, initFailure = false, armFailure = false;
struct Lock {};
uint32_t nowMs() { return millis(); }
#endif
void PUMP_IRAM writeOff() {
#ifdef ARDUINO
  gpio_ll_set_level(&GPIO, board_config::kRelayIn1Pin, 0);
#else
  digitalWrite(board_config::kRelayIn1Pin, LOW);
#endif
}
void PUMP_IRAM stop(StopCause cause) {
  writeOff();
  if (running) { stopEvent.cause = cause; stopEvent.generation = activeRun.generation; }
  running = false;
}
// Lock held in both ISR and owner context. No queued per-run callback or stale snapshot.
void PUMP_IRAM checkGuard(uint32_t now) {
  if (!running) return;
  if (static_cast<uint32_t>(now - activeRun.startedMs) >= activeRun.durationMs)
    stop(StopCause::Timeout);
  else if (static_cast<uint32_t>(now - leaseStartedMs) >= leaseDurationMs)
    stop(StopCause::SafetyLeaseExpired);
}
#ifdef ARDUINO
bool PUMP_IRAM alarmCallback(gptimer_handle_t, const gptimer_alarm_event_data_t*, void*) {
  portENTER_CRITICAL_ISR(&mux);
  checkGuard(nowMs());
  portEXIT_CRITICAL_ISR(&mux);
  return false;
}
bool initializeGuard() {
  if (timer) return guardReady;  // A partial failure never authorizes starts.
  gptimer_config_t config = {};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = 1000000;
  if (gptimer_new_timer(&config, &timer) != ESP_OK) return false;
  gptimer_event_callbacks_t callbacks = {}; callbacks.on_alarm = alarmCallback;
  if (gptimer_register_event_callbacks(timer, &callbacks, nullptr) != ESP_OK) return false;
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = 10000; alarm.reload_count = 0; alarm.flags.auto_reload_on_alarm = true;
  if (gptimer_set_alarm_action(timer, &alarm) != ESP_OK) return false;
  if (gptimer_enable(timer) != ESP_OK) return false;
  return gptimer_start(timer) == ESP_OK;
}
#endif
bool pollLocked() {
  if (running && static_cast<uint32_t>(nowMs() - activeRun.startedMs) >= activeRun.durationMs) {
    stop(StopCause::Timeout); return true;
  }
  return false;
}
}
void begin(QualificationPolicy policy) {
  { Lock lock; (void)lock; qualification = policy; stop(StopCause::Off);
    stopEvent = StopEvent{}; leaseDurationMs = 0; }
  // Preload OFF before OUTPUT; GPIO ownership stays in this module.
  pinMode(board_config::kRelayIn1Pin, OUTPUT);
#ifdef ARDUINO
  const bool ready = initializeGuard();
#else
  const bool ready = !initFailure;
#endif
  { Lock lock; (void)lock; guardReady = ready; }
}
bool poll() { Lock lock; (void)lock; return pollLocked(); }
bool isOn() { Lock lock; (void)lock; pollLocked(); return running; }
uint32_t remainingMs() {
  Lock lock; (void)lock; pollLocked();
  const uint32_t elapsed = static_cast<uint32_t>(nowMs() - activeRun.startedMs);
  if (running && elapsed >= activeRun.durationMs) stop(StopCause::Timeout);
  return running ? activeRun.durationMs - elapsed : 0;
}
Run run() { Lock lock; (void)lock; return activeRun; }
StopEvent takeStopEvent() {
  Lock lock; (void)lock; pollLocked();
  const auto event = stopEvent; stopEvent = StopEvent{}; return event;
}
#if defined(F31D_GUARD_TEST) && F31D_GUARD_TEST == 1
StopEvent guardTestStopEvent() { Lock lock; (void)lock; return stopEvent; }
#endif
Result start(uint32_t seconds) {
  Lock lock; (void)lock;
  const bool wasRunning = running; pollLocked();
  if (seconds < 1 || seconds > 600) return Result::Invalid;
  if (wasRunning) return Result::AlreadyOn;
  if (seconds > qualification.ceilingSeconds()) return Result::QualificationLimit;
#ifndef ARDUINO
  if (armFailure) { writeOff(); return Result::GuardUnavailable; }
#endif
  if (!guardReady) { writeOff(); return Result::GuardUnavailable; }
  // The periodic alarm is already successfully configured/enabled/running.
  // Arming this generation is a non-failing state commit under the ISR lock.
  activeRun.startedMs = nowMs(); activeRun.durationMs = seconds * 1000u;
  ++activeRun.generation; stopEvent = StopEvent{};
  leaseStartedMs = activeRun.startedMs; leaseDurationMs = kLeaseMs;
  running = true;
#ifdef ARDUINO
  gpio_ll_set_level(&GPIO, board_config::kRelayIn1Pin, 1);
#else
  digitalWrite(board_config::kRelayIn1Pin, HIGH);
#endif
  return Result::On;
}
bool renewSafetyLease(uint32_t sampledAtMs, uint32_t maxReadingAgeMs) {
  Lock lock; (void)lock;
  const uint32_t now = nowMs();
#ifdef ARDUINO
  checkGuard(now);  // A late owner cannot resurrect an already expired lease.
#else
  // Old regression fake clocks do not dispatch interrupts. Guard suite enables
  // real-time lease semantics and drives the ISR explicitly, without owner calls.
  if (dispatchGuard) checkGuard(now);
#endif
  const uint32_t age = static_cast<uint32_t>(now - sampledAtMs);
  if (!running || !guardReady || !maxReadingAgeMs || maxReadingAgeMs >= 0x80000000u || age > maxReadingAgeMs) return false;
  const uint32_t freshnessRemainingMs = maxReadingAgeMs - age + 1;
  leaseStartedMs = now;
  leaseDurationMs = freshnessRemainingMs < kLeaseMs ? freshnessRemainingMs : kLeaseMs;
  return true;
}
Result command(const char* text) {
  if (text && strcmp(text, "ON") == 0) return start(5);
  Lock lock; (void)lock; pollLocked();
  if (!text) return Result::Invalid;
  if (strcmp(text, "OFF") == 0) { stop(StopCause::Off); return Result::Off; }
  if (strcmp(text, "STATUS") == 0) return Result::Status;
  return Result::Invalid;
}
#ifndef ARDUINO
void testGuardEnable(bool enabled) { dispatchGuard = enabled; }
void testGuardFailure(bool initialization, bool arming) { initFailure = initialization; armFailure = arming; }
void testGuardInterrupt() { checkGuard(nowMs()); }
#endif
}  // namespace pump
