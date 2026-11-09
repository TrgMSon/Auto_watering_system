#include "pump.h"
#include "autonomous_feature.h"
#include <Arduino.h>
#include <string.h>
#include "board_config.h"
#ifdef ARDUINO
#include "driver/gptimer.h"
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "esp_timer.h"
#define PUMP_IRAM IRAM_ATTR
#if WATERING_AUTONOMOUS_TEST
#include "esp_arduino_version.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "esp_private/gptimer.h"
#include "esp_memory_utils.h"
#if ESP_ARDUINO_VERSION != ESP_ARDUINO_VERSION_VAL(3, 3, 12) || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(5, 5, 5)
#error Experimental pump guard requires audited Arduino ESP32 3.3.12 / ESP-IDF 5.5.5
#endif
#if !CONFIG_IDF_TARGET_ESP32 || !CONFIG_GPTIMER_ISR_HANDLER_IN_IRAM || !CONFIG_GPTIMER_OBJ_CACHE_SAFE || !CONFIG_ESP_TIMER_IN_IRAM
#error Experimental pump guard requires audited ESP32 ISR, object and clock cache-safe placement
#endif
#define PUMP_DRAM DRAM_ATTR
#endif
#else
#define PUMP_IRAM
#endif
#ifndef PUMP_DRAM
#define PUMP_DRAM
#endif
namespace pump {
namespace {
constexpr uint32_t kLeaseMs = 250;
#if WATERING_AUTONOMOUS_TEST
// Owner-task-only diagnostic: setup()/loop() share Arduino loopTask.
// Never accessed by ISR or BLE/MQTT workers; not a cross-task status API.
enum class GuardInitialization { Unknown, Ready, Failed };
GuardInitialization guardInitialization = GuardInitialization::Unknown;
#endif
PUMP_DRAM bool running = false, guardReady = false;
PUMP_DRAM Run activeRun;
PUMP_DRAM StopEvent stopEvent;
QualificationPolicy qualification;
PUMP_DRAM uint32_t leaseStartedMs = 0, leaseDurationMs = 0;
#ifdef ARDUINO
PUMP_DRAM portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
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
#if WATERING_AUTONOMOUS_TEST
bool guardAttempted = false;
bool rejectGuard(bool activationAttempted) {
  if (!timer) return false;
  if (activationAttempted) {
    // enable() may have changed FSM before returning an error. stop()/disable()
    // tolerate INIT/ENABLE via INVALID_STATE; del_timer remains the final check.
    const esp_err_t stopped = gptimer_stop(timer);
    const esp_err_t disabled = gptimer_disable(timer);
    if ((stopped != ESP_OK && stopped != ESP_ERR_INVALID_STATE) ||
        (disabled != ESP_OK && disabled != ESP_ERR_INVALID_STATE)) return false;
  }
  // Keep ownership of an unreclaimable handle. Never allocate/retry around it.
  if (gptimer_del_timer(timer) == ESP_OK) timer = nullptr;
  return false;
}
bool cacheSafePumpState() {
  return esp_ptr_in_iram(reinterpret_cast<const void*>(alarmCallback)) &&
      esp_ptr_in_iram(reinterpret_cast<const void*>(checkGuard)) &&
      esp_ptr_in_iram(reinterpret_cast<const void*>(stop)) &&
      esp_ptr_in_iram(reinterpret_cast<const void*>(writeOff)) &&
      esp_ptr_in_iram(reinterpret_cast<const void*>(nowMs)) &&
      esp_ptr_in_iram(reinterpret_cast<const void*>(esp_timer_get_time)) &&
      esp_ptr_in_dram(&running) && esp_ptr_in_dram(&activeRun) &&
      esp_ptr_in_dram(&stopEvent) && esp_ptr_in_dram(&leaseStartedMs) &&
      esp_ptr_in_dram(&leaseDurationMs) && esp_ptr_in_dram(&mux);
}
#endif
bool initializeGuard() {
#if WATERING_AUTONOMOUS_TEST
  if (guardAttempted) return guardReady; // Boot-latched failures, no resource churn.
  guardAttempted = true;
  if (!cacheSafePumpState()) return false;
  gptimer_config_t config = {};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = 1000000;
  config.flags.intr_shared = false;
  if (gptimer_new_timer(&config, &timer) != ESP_OK) return rejectGuard(false);
  if (!timer || !esp_ptr_internal(timer)) return rejectGuard(false);
  gptimer_event_callbacks_t callbacks = {}; callbacks.on_alarm = alarmCallback;
  if (gptimer_register_event_callbacks(timer, &callbacks, nullptr) != ESP_OK) return rejectGuard(false);
  // register_event_callbacks lazily allocates the interrupt, still disabled.
  intr_handle_t interrupt = nullptr;
  if (gptimer_get_intr_handle(timer, &interrupt) != ESP_OK || !interrupt ||
      !esp_ptr_internal(interrupt)) return rejectGuard(false);
  if (esp_intr_set_in_iram(interrupt, true) != ESP_OK) return rejectGuard(false);
  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = 10000; alarm.reload_count = 0; alarm.flags.auto_reload_on_alarm = true;
  if (gptimer_set_alarm_action(timer, &alarm) != ESP_OK) return rejectGuard(false);
  if (gptimer_enable(timer) != ESP_OK) return rejectGuard(true);
  if (gptimer_start(timer) != ESP_OK) return rejectGuard(true);
  return true;
#else
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
#endif
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
#if WATERING_AUTONOMOUS_TEST
  guardInitialization = GuardInitialization::Unknown;
#endif
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
#if WATERING_AUTONOMOUS_TEST
  guardInitialization = ready ? GuardInitialization::Ready : GuardInitialization::Failed;
#endif
}
#if WATERING_AUTONOMOUS_TEST
const char* guardInitializationStatus() {
  return guardInitialization == GuardInitialization::Ready ? "READY" :
      guardInitialization == GuardInitialization::Failed ? "FAILED" : "UNKNOWN";
}
#endif
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
