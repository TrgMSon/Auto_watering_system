#pragma once
#include "ble_auth.h"
#include <string.h>
namespace ble_wifi {
// Implementations must never block
// waiting for radio association, nor persist candidate in startRam().
struct Radio {
  enum class State { Pending, CandidateIp, Failed };
  virtual ~Radio() {}
  virtual bool startRam(const ble_commands::WifiRequest& candidate) = 0;
  // CandidateIp must refer to this attempt's SSID and a valid nonzero STA IP.
  virtual State poll() = 0;
  virtual void stopCandidate() = 0;
};
enum class Persistence { Verified, NotCommitted, Unconfirmed };
struct Storage {
  virtual ~Storage() {}
  // Unconfirmed may have changed flash. Never infer rollback or retry automatically.
  virtual Persistence commitVerified(const ble_commands::WifiRequest& candidate) = 0;
};
class Attempt {
 public:
  // A real SDK/storage service supplies millis (or an equivalent monotonic
  // function) so completion time includes time spent in checked storage APIs.
  explicit Attempt(uint32_t (*monotonicNow)()=nullptr):monotonicNow_(monotonicNow){}
  enum class State { Idle, Pending, Connecting, Done };
  ~Attempt() { candidate_.clear(); }
  bool reserve(uint64_t generation, const ble_commands::WifiAuthentication& auth,
      bool resultsSubscribed, bool congested, uint16_t mtu, uint32_t now) {
    if (state_!=State::Idle || !generation || auth.generation!=generation || auth.code!=ble_commands::Code::Ok ||
        auth.missingKey || auth.securityDenied || auth.transportDenied ||
        !resultsSubscribed || congested || mtu<3 || size_t(mtu-3)<kResultMax) return false;
    persistence_=Persistence::NotCommitted; evidence_=auth.encryptedWriteGeneration; candidate_=auth.request; generation_=generation; queued_=now; state_=State::Pending;
    return true;
  }
  // Called by a separate provisioning service, never the irrigation owner or callback.
  // No queue/task is created by this logic. Exactly one outstanding attempt.
  void service(uint32_t now, uint64_t current, Radio& radio, Storage& storage,
      const ble_commands::WifiSecurity& security) {
    if (state_==State::Done) {
      if (generation_!=current || uint32_t(now-finished_)>=3000) reset();
      return;
    }
    if (state_==State::Pending) {
      if (generation_!=current || !security.permitsWrite(current,evidence_) || uint32_t(now-queued_)>=1000) { reset(); return; }
      started_=now; state_=State::Connecting;
      if (!radio.startRam(candidate_)) { radio.stopCandidate(); finish(false,now); return; }
    }
    if (state_!=State::Connecting) return;
    // After admission, finish bounded attempt even if disconnected. Never send
    // its outcome to a new generation. No automatic retry or previous-AP restore.
    if (uint32_t(now-started_)>=30000) { radio.stopCandidate(); finish(false,now); return; }
    const auto status=radio.poll();
    if (status==Radio::State::Failed) { radio.stopCandidate(); finish(false,now); }
    else if (status==Radio::State::CandidateIp) {
      persistence_=storage.commitVerified(candidate_);
      const bool ok=persistence_==Persistence::Verified;
      if (!ok) radio.stopCandidate();
      finish(ok,now);
    }
    if (state_==State::Done && generation_!=current) reset();
  }
  bool result(uint64_t current, uint32_t now, ble_commands::Code& code) {
    if (state_!=State::Done) return false;
    // Delivery timeout starts at completion, not at reservation or connection.
    if (generation_!=current || uint32_t(now-finished_)>=3000) { reset(); return false; }
    code=code_; return true;
  }
  void release() { if (state_==State::Done) reset(); }
  State state() const { return state_; }
  Persistence persistence() const { return persistence_; }
  static size_t resultJson(ble_commands::Code code, char* out, size_t capacity) {
    const char* wire=code==ble_commands::Code::WifiOk ? "{\"op\":\"WIFI\",\"code\":\"WIFI_OK\"}" :
        code==ble_commands::Code::WifiFail ? "{\"op\":\"WIFI\",\"code\":\"WIFI_FAIL\"}" : nullptr;
    if (!out || !capacity) return 0;
    out[0]=0; if (!wire || capacity<=strlen(wire)) return 0;
    strcpy(out,wire); return strlen(out);
  }
 private:
  static constexpr size_t kResultMax=sizeof("{\"op\":\"WIFI\",\"code\":\"WIFI_FAIL\"}")-1;
  void finish(bool ok, uint32_t now) {
    candidate_.clear(); code_=ok?ble_commands::Code::WifiOk:ble_commands::Code::WifiFail;
    state_=State::Done; finished_=monotonicNow_?monotonicNow_():now;
  }
  void reset() { candidate_.clear(); state_=State::Idle; generation_=0; code_=ble_commands::Code::None; }
  Persistence persistence_=Persistence::NotCommitted;
  uint32_t (*monotonicNow_)()=nullptr;
  State state_=State::Idle; ble_commands::WifiRequest candidate_;
  uint64_t evidence_=0;
  uint64_t generation_=0; uint32_t queued_=0, started_=0, finished_=0;
  ble_commands::Code code_=ble_commands::Code::None;
};
}
