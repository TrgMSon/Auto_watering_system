#include "ble_command_bridge.h"
#include "mqtt_commands.h"
#include <Arduino.h>
namespace ble_command_bridge {
namespace {
ble_commands::Dispatch dispatch;
plant_mqtt::Commands mqtt;
uint64_t mqttEpoch = 0, reconciledEpoch = 0;
uint64_t offEpoch = 0;
bool mqttOnline = false;
void reconcileMqtt() {
  if (mqttEpoch == reconciledEpoch) return;
  // Any connection change invalidates previous unadmitted work, even if both
  // disconnect and reconnect happened before the next owner/worker pass.
  mqtt.connection(false);
  if (mqttOnline) mqtt.connection(true);
  reconciledEpoch = mqttEpoch;
}
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
    ble_commands::Code code, uint32_t now, uint64_t ingressEpoch) {
  Lock lock; (void)lock;
  reconcileMqtt();
  if ((command.op == ble_commands::Op::On || command.op == ble_commands::Op::Config) && ingressEpoch != UINT64_MAX && ingressEpoch != offEpoch) return false;
  const bool reserved = dispatch.reserve(generation, transaction, command, code, now);
  if (reserved && command.op == ble_commands::Op::Off && code == ble_commands::Code::Ok) {
    ++offEpoch;
    mqtt.cancel("superseded by BLE OFF");
  }
  return reserved;
}
bool result(ble_commands::Dispatch::Slot& slot) { Lock lock; (void)lock; return dispatch.result(slot); }
void release(uint32_t ticket) { Lock lock; (void)lock; dispatch.release(ticket); }
void expire(uint32_t now) { Lock lock; (void)lock; dispatch.expire(now); }
void serviceOwner(irrigation::System& system, ble_commands::Clock* clock) {
  ble_commands::Dispatch::Slot request;
  int mqttSlot = -1;
  plant_mqtt::PumpCommand mqttRequest;
  bool bleClaim = false;
  {
    Lock lock; (void)lock;
    reconcileMqtt();
    dispatch.expire(millis());
    if (dispatch.offPending()) mqtt.cancel("superseded by BLE OFF");
    if (mqtt.offPending()) dispatch.cancelOn();
    // A single admission per pass. OFF on either transport precedes all ON.
    if (dispatch.offPending()) bleClaim = dispatch.claim(request);
    else if (mqtt.offPending()) mqttSlot = mqtt.claim(millis());
    else {
      bleClaim = dispatch.claim(request);
      if (!bleClaim) mqttSlot = mqtt.claim(millis());
    }
    if (mqttSlot >= 0) mqttRequest = mqtt.record(mqttSlot).command;
  }
  if (mqttSlot >= 0) {
    if (mqttRequest.isConfig) {
      const char* message = plant_mqtt::applyConfiguration(system, mqttRequest);
      Lock lock; (void)lock; mqtt.completeConfiguration(mqttSlot, message); return;
    }
    const auto outcome = mqttRequest.on ? system.startManual(mqttRequest.seconds) : system.stopManual();
    const char* detail = !mqttRequest.on && system.status().modeStorageError ? "OFF_APPLIED_MODE_SAVE_FAILED" :
        outcome == irrigation::Result::Unsafe ? watering::name(system.status().decision.reason) : nullptr;
    Lock lock; (void)lock; mqtt.complete(mqttSlot, outcome, detail); return;
  }
  if (!bleClaim) return;
  // Claim is the admission point. Disconnect after this point does not cancel
  // an admitted run; its outcome is discarded if its generation is obsolete.
  const auto outcome = ble_commands::execute(system, request.command, clock);
  { Lock lock; (void)lock; dispatch.complete(request.ticket, outcome); }
}
}
namespace plant_mqtt {
uint64_t commandConnection(bool online) {
  // Callback copies connection metadata only. Cache reconciliation happens in
  // worker/owner context under the same admission lock.
  ble_command_bridge::Lock lock; (void)lock;
  ble_command_bridge::mqttOnline = online;
  return ++ble_command_bridge::mqttEpoch;
}
uint64_t commandIngressEpoch() { ble_command_bridge::Lock lock; (void)lock; return ble_command_bridge::offEpoch; }
uint64_t commandConnectionGeneration() { ble_command_bridge::Lock lock; (void)lock; return ble_command_bridge::mqttEpoch; }
static bool queueLocked(const PumpCommand& c, uint32_t now, uint64_t ingressEpoch) {
  ble_command_bridge::reconcileMqtt();
  ble_command_bridge::mqtt.reclaim(now);
  const bool known = ble_command_bridge::mqtt.contains(c.id);
  if (!known && !c.off() && ingressEpoch != UINT64_MAX && ingressEpoch != ble_command_bridge::offEpoch) return false;
  const bool accepted = ble_command_bridge::mqtt.submit(c, now);
  if (accepted && !known && c.off()) ++ble_command_bridge::offEpoch;
  if (accepted && ble_command_bridge::mqtt.offPending()) ble_command_bridge::dispatch.cancelOn();
  return accepted;
}
bool queueCommand(const PumpCommand& c, uint32_t now, uint64_t ingressEpoch) {
  ble_command_bridge::Lock lock; (void)lock;
  return queueLocked(c,now,ingressEpoch);
}
bool queueCommandBatch(const PumpCommand* commands, const uint64_t* epochs, size_t count, uint32_t now,
    uint64_t connectionGeneration) {
  ble_command_bridge::Lock lock; (void)lock;
  // Validate at the admission boundary, including disconnect+reconnect while
  // the worker parsed a detached ingress snapshot. Never replay that batch.
  if (connectionGeneration != UINT64_MAX &&
      (!ble_command_bridge::mqttOnline || connectionGeneration != ble_command_bridge::mqttEpoch)) return false;
  for (size_t i=0;i<count && i<4;++i) queueLocked(commands[i],now,epochs[i]);
  return true;
}
bool commandAck(uint32_t now, int& slot, char* bytes, size_t capacity, const char* identity) {
  ble_command_bridge::Lock lock; (void)lock;
  ble_command_bridge::reconcileMqtt();
  return ble_command_bridge::mqtt.ack(now, slot, bytes, capacity, identity);
}
void commandAckAttempt(int slot, uint32_t now, bool inFlight) {
  ble_command_bridge::Lock lock; (void)lock; ble_command_bridge::mqtt.attempted(slot, now, inFlight);
}
void ackAccepted(int slot, uint32_t now) { ble_command_bridge::Lock lock; (void)lock; ble_command_bridge::mqtt.accepted(slot, now); }
void commandAckSettled(int slot, uint32_t now) { ble_command_bridge::Lock lock; (void)lock; ble_command_bridge::mqtt.finishAttempt(slot, now); }
}
