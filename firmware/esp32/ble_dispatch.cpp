#include "ble_dispatch.h"
namespace ble_commands {
bool Dispatch::offPending() const {
  return slots_[0].state == State::Pending;
}
void Dispatch::cancelOn() {
  for (auto& s : slots_) if (s.state == State::Pending && (s.command.op == Op::On || s.command.op == Op::Config)) {
    s.state = State::Done; s.code = Code::None;
  }
}
void Dispatch::invalidate() {
  active_ = 0;
  for (auto& s : slots_) if (s.state != State::Admitted) s = Slot{};
}
uint64_t Dispatch::connect(uint16_t id) {
  if (disabled_ || active_) return 0;
  if (!++serial_) ++serial_;
  active_ = serial_; connection_ = id; return active_;
}
void Dispatch::disconnect(uint16_t id) { if (generation(id)) invalidate(); }
void Dispatch::disable() { disabled_ = true; invalidate(); }
bool Dispatch::reserve(uint64_t generation, uint32_t transaction, const Command& command,
    Code authentication, uint32_t now) {
  if (!current(generation) || command.op == Op::Unknown) return false;
  // Slot zero is reserved for OFF, including its result. ON saturation cannot
  // consume OFF's tracking capacity. Other slots are FIFO ON/TIME/result storage.
  const unsigned first = command.op == Op::Off ? 0 : 1;
  const unsigned end = command.op == Op::Off ? 1 : kSlots;
  for (unsigned i = first; i < end; ++i) if (slots_[i].state == State::Free) {
    Slot& s = slots_[i]; s.generation = generation; s.transaction = transaction;
    s.connection = connection_; s.command = command; s.queuedMs = now;
    if (!++ticket_) ++ticket_;
    s.ticket = ticket_; s.code = authentication;
    s.state = authentication == Code::Ok ? State::Pending : State::Done;
    if (command.op == Op::Off && s.state == State::Pending) {
      for (unsigned j = 1; j < kSlots; ++j) if (slots_[j].state == State::Pending && (slots_[j].command.op == Op::On || slots_[j].command.op == Op::Config)) {
        slots_[j].state = State::Done; slots_[j].code = Code::None; // Superseded, no wire code.
      }
    }
    return true;
  }
  return false;
}
bool Dispatch::claim(Slot& out) {
  unsigned best = kSlots;
  for (unsigned i = 0; i < kSlots; ++i) {
    auto& s = slots_[i];
    if (s.state != State::Pending || !current(s.generation)) continue;
    if (i == 0) { best = 0; break; }
    if (best == kSlots || static_cast<int32_t>(s.ticket - slots_[best].ticket) < 0) best = i;
  }
  if (best == kSlots) return false;
  slots_[best].state = State::Admitted; out = slots_[best]; return true;
}
void Dispatch::complete(uint32_t ticket, Code code) {
  for (auto& s : slots_) if (s.state == State::Admitted && s.ticket == ticket) {
    if (!current(s.generation)) s = Slot{};
    else { s.code = code; s.state = State::Done; }
    return;
  }
}
bool Dispatch::result(Slot& out) const {
  for (const auto& s : slots_) if (s.state == State::Done && current(s.generation)) { out = s; return true; }
  return false;
}
void Dispatch::release(uint32_t ticket) {
  for (auto& s : slots_) if (s.ticket == ticket && s.state == State::Done) { s = Slot{}; return; }
}
void Dispatch::expire(uint32_t now) {
  for (auto& s : slots_) if (s.state == State::Pending && uint32_t(now - s.queuedMs) >= 1000) {
    s.state = State::Done; s.code = Code::None;
  }
}
Code execute(irrigation::System& system, const Command& command, Clock* clock) {
  if (command.op == Op::Config) {
    const auto& r = command.config;
    if (validateConfig(r) != Code::Ok) return Code::OutOfRange;
    auto candidate = system.configurationStatus().active();
    candidate.soilStart = r.minPercentTenths / 10.0f;
    candidate.soilStop = r.maxPercentTenths / 10.0f;
    candidate.waterMin = r.minWaterPercentTenths / 10.0f;
    candidate.maxRunSeconds = r.maxDurationSeconds;
    switch (system.applyRemoteConfiguration(candidate, r.autoMode != 0)) {
      case irrigation::RemoteConfigResult::Ok: return Code::Ok;
      case irrigation::RemoteConfigResult::Invalid: return Code::OutOfRange;
      case irrigation::RemoteConfigResult::PumpRunning: return Code::PumpBusy;
      case irrigation::RemoteConfigResult::OtaInProgress: return Code::OtaInProgress;
      case irrigation::RemoteConfigResult::StorageError: return Code::StorageError;
    }
    return Code::None;
  }
  if (command.op == Op::Time) {
    if (!validUnix(command.unixSeconds)) return Code::InvalidTime;
    return clock && clock->setUtc(command.unixSeconds) ? Code::Ok : Code::None;
  }
  const auto result = command.op == Op::Off ? system.stopManual() :
      command.op == Op::On ? system.startManual(command.seconds) : irrigation::Result::Invalid;
#if WATERING_AUTONOMOUS_TEST
  if(result!=irrigation::Result::Accepted && system.status().autonomousBlock==watering::Block::StorageFault)
    return Code::StorageError;
#endif
  switch (result) {
    case irrigation::Result::Accepted: return Code::Ok;
    case irrigation::Result::AlreadyOn: return Code::PumpBusy;
    case irrigation::Result::OtaInProgress: return Code::OtaInProgress;
    case irrigation::Result::InvalidDuration:
    case irrigation::Result::ExceedsConfiguredMaximum:
    case irrigation::Result::QualificationLimit: return Code::OutOfRange;
    case irrigation::Result::Unsafe:
      switch (system.status().decision.reason) {
        case watering::Reason::SoilInvalid:
        case watering::Reason::DistanceInvalid:
        case watering::Reason::Stale: return Code::SensorError;
        case watering::Reason::LowWater: return Code::LowWater;
        default: return Code::None;
      }
    default: return Code::None; // Lock, cooldown, mode, guard/config: mapping gap.
  }
}
}
