#pragma once
#include "irrigation.h"
#include <cstring>
#include <cstdio>
namespace plant_mqtt {
constexpr size_t kCommandBytes = 512;
constexpr uint32_t kDedupRetentionMs = 60000;
struct PumpCommand { char id[65] = {}; bool on = false; uint32_t seconds = 0; };
// Strict, bounded ASCII subset of the backend's literal JSON. Unknown fields,
// escapes, duplicate fields and numeric alternatives are rejected, never coerced.
bool parsePump(const char* bytes, size_t size, const char* identity, PumpCommand& out);
class Commands {
 public:
  struct Record {
    PumpCommand command;
    enum State { Free, Pending, Admitted, Done } state = Free;
    uint64_t generation = 0;
    uint32_t queued = 0, lastAck = 0;
    uint32_t ackEndedMs = 0;
    unsigned attempts = 0;
    bool ackFinished = false, ackInFlight = false;
    bool success = false;
    const char* message = "rejected";
  };
  void connection(bool online) { ++generation_; online_ = online; cancel("disconnected"); }
  bool submit(const PumpCommand& command, uint32_t now);
  void reclaim(uint32_t now);
  bool contains(const char* id) const {
    for (const auto& r : records_) if (r.state != Record::Free && !strcmp(r.command.id,id)) return true;
    return false;
  }
  void cancel(const char* reason);
  bool offPending() const;
  int claim(uint32_t now);
  void complete(int slot, irrigation::Result result, const char* detail = nullptr);
  bool ack(uint32_t now, int& slot, char* bytes, size_t capacity, const char* identity);
  void attempted(int slot, uint32_t now, bool inFlight = false);
  void accepted(int slot, uint32_t now);
  void finishAttempt(int slot, uint32_t now);
  const Record& record(unsigned i) const { return records_[i]; }
 private:
  // Slot zero is reserved for OFF. Only ACK-finished Done records can expire.
  Record records_[16];
  uint64_t generation_ = 0;
  bool online_ = false;
};
// All transitions below share the BLE dispatch lock. Only serviceOwner mutates System.
uint64_t commandConnection(bool online);
uint64_t commandIngressEpoch();
uint64_t commandConnectionGeneration();
bool queueCommand(const PumpCommand& command, uint32_t now, uint64_t ingressEpoch = UINT64_MAX);
bool queueCommandBatch(const PumpCommand* commands, const uint64_t* epochs, size_t count, uint32_t now,
    uint64_t connectionGeneration = UINT64_MAX);
bool commandAck(uint32_t now, int& slot, char* bytes, size_t capacity, const char* identity);
void commandAckAttempt(int slot, uint32_t now, bool inFlight);
void ackAccepted(int slot, uint32_t now);
void commandAckSettled(int slot, uint32_t now);
}
