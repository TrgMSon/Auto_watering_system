#pragma once
#include "watering.h"
#include <stddef.h>
#include <stdint.h>
namespace autonomous_storage {
constexpr size_t kRecordSize = 40;
enum class Read { Ok, Missing, Error };
struct Storage {
  virtual ~Storage() {}
  virtual Read read(uint8_t* out, size_t capacity, size_t& size) = 0;
  // Success means set_blob AND commit succeeded. Failure may still change flash.
  virtual bool writeCommit(const uint8_t* bytes, size_t size) = 0;
};
enum Flag : uint32_t { Episode=1, Pending=2, Cooldown=4, Waiting=8, Maintenance=16, Exhausted=32 };
struct Record {
  uint64_t sequence = 0, request = 0;
  uint32_t flags = 0, cycle = 0, total = 0;
};
uint32_t crc32(const uint8_t*, size_t);
bool valid(const Record&);
void encode(const Record&, uint8_t (&)[kRecordSize]);
bool decode(const uint8_t*, size_t, Record&);
enum class Result { Ok, Missing, Invalid, StorageFault, MaintenanceRequired,
                    BudgetExhausted, Cooldown, WaitingRetry, Duplicate, Conflict, Unsafe };
struct Permit {
  Result result = Result::StorageFault;
  uint64_t sequence = 0, request = 0;
  bool allowed() const { return result == Result::Ok; }
};
// Single-owner, isolated B2 component. No pump, transport or mode calls.
class Ledger {
 public:
  explicit Ledger(Storage& storage) : storage_(storage) {}
  Ledger(const Ledger&) = delete;
  Ledger& operator=(const Ledger&) = delete;
  Result load(uint32_t now);
  // Explicit local maintenance only; never call automatically for missing data.
  Result maintenanceInitialize(bool pumpOff, uint32_t now);
  // proposal is a staged B1 copy, not permission to operate GPIO.
  Permit reserve(const watering::AutonomousDecision& proposal, uint64_t expectedSequence,
                 uint64_t request, uint32_t now);
  Result complete(const Permit&, bool pumpOff, uint32_t now, bool maintenanceRequired = false);
  // Caller supplies fresh 30s wet/safety evidence; component checks accounting/OFF.
  Result endEpisode(bool pumpOff, bool wetConfirmed, uint32_t now);
  Result markMaintenance(bool pumpOff, uint32_t now);
  bool restoreController(watering::AutonomousController&, uint32_t now) const;
  const Record& record() const { return record_; }
  uint32_t cooldownRemaining(uint32_t now) const {
    const uint32_t elapsed=now-cooldownAt_;
    return (record_.flags&Cooldown)&&elapsed<30000 ? 30000-elapsed : 0;
  }
#if WATERING_DEMO_RETRY
  uint32_t committedRetryMs() const { return episodeRetryMs_; }
#endif
  bool blocked() const { return !loaded_ || fault_ || (record_.flags & (Pending|Maintenance|Exhausted)); }
 private:
  Storage& storage_;
  Record record_;
  bool loaded_ = false, fault_ = false, attemptedLoad_ = false, issued_ = false;
  uint32_t cooldownAt_ = 0, retryAt_ = 0;
#if WATERING_DEMO_RETRY
  bool demoEligible_ = false; // Granted only by first successful clean boot load.
  uint32_t episodeRetryMs_ = 1800000;
#endif
  Result commit(Record candidate);
  Result gate() const;
};
}  // namespace autonomous_storage
