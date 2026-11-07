#pragma once
#include "ble_auth.h"
#include "ble_clock.h"
#include "irrigation.h"
namespace ble_commands {
// External serialization protects these small value-only transitions. Never
// hold that lock while executing System or a BLE API.
class Dispatch {
 public:
  static constexpr unsigned kSlots = 4;
  enum class State { Free, Pending, Admitted, Done };
  struct Slot {
    State state = State::Free;
    uint64_t generation = 0;
    uint32_t ticket = 0, transaction = 0, queuedMs = 0;
    uint16_t connection = 0;
    Command command;
    Code code = Code::None;
  };
  uint64_t connect(uint16_t id);
  void disconnect(uint16_t id);
  void disable();
  uint64_t generation(uint16_t id) const { return active_ && id == connection_ ? active_ : 0; }
  bool current(uint64_t generation) const { return generation && generation == active_; }
  bool reserve(uint64_t generation, uint32_t transaction, const Command& command,
      Code authentication, uint32_t now);
  bool claim(Slot& out);  // OFF first; admission linearizes here in the owner.
  void complete(uint32_t ticket, Code code);
  bool result(Slot& out) const;
  void release(uint32_t ticket);
  void expire(uint32_t now);
  bool offPending() const;
  void cancelOn();
 private:
  void invalidate();
  Slot slots_[kSlots];
  uint64_t serial_ = 0, active_ = 0;
  uint32_t ticket_ = 0;
  uint16_t connection_ = 0;
  bool disabled_ = false;
};
// Only the irrigation owner invokes execute(). None means a contract mapping
// gap: report ATT failure, never invent a business code or a success.
Code execute(irrigation::System& system, const Command& command, Clock* clock = nullptr);
}
