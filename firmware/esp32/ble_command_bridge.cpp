#include "ble_command_bridge.h"
#include <Arduino.h>
namespace ble_command_bridge {
namespace {
ble_commands::Dispatch dispatch;
#ifdef ARDUINO
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Lock { Lock() { portENTER_CRITICAL(&mux); } ~Lock() { portEXIT_CRITICAL(&mux); } };
#else
struct Lock {};
#endif
}
uint64_t connect(uint16_t id) { Lock lock; (void)lock; return dispatch.connect(id); }
void disconnect(uint16_t id) { Lock lock; (void)lock; dispatch.disconnect(id); }
void disable() { Lock lock; (void)lock; dispatch.disable(); }
uint64_t generation(uint16_t id) { Lock lock; (void)lock; return dispatch.generation(id); }
bool current(uint64_t generation) { Lock lock; (void)lock; return dispatch.current(generation); }
bool reserve(uint64_t generation, uint32_t transaction, const ble_commands::Command& command,
    ble_commands::Code code, uint32_t now) {
  Lock lock; (void)lock; return dispatch.reserve(generation, transaction, command, code, now);
}
bool result(ble_commands::Dispatch::Slot& slot) { Lock lock; (void)lock; return dispatch.result(slot); }
void release(uint32_t ticket) { Lock lock; (void)lock; dispatch.release(ticket); }
void expire(uint32_t now) { Lock lock; (void)lock; dispatch.expire(now); }
void serviceOwner(irrigation::System& system, ble_commands::Clock* clock) {
  ble_commands::Dispatch::Slot request;
  {
    Lock lock; (void)lock;
    dispatch.expire(millis());
    if (!dispatch.claim(request)) return;
  }
  // Claim is the admission point. Disconnect after this point does not cancel
  // an admitted run; its outcome is discarded if its generation is obsolete.
  const auto outcome = ble_commands::execute(system, request.command, clock);
  { Lock lock; (void)lock; dispatch.complete(request.ticket, outcome); }
}
}
