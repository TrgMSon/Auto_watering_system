#pragma once
#include "autonomous_feature.h"
#if WATERING_AUTONOMOUS_TEST
#include "autonomous_storage.h"
#include "pump.h"
namespace experimental_watering {
enum class Outcome { Started, Busy, Unsafe, Locked, Cooldown, StorageFault, ActuatorFault };
class Owner {
 public:
  explicit Owner(const watering::Config& config) : config_(config), controller_(config), safety_(config), freshness_(config.maxReadingAgeMs) {}
  void setGate(uint64_t (*capture)(), bool (*allowed)(uint64_t), pump::Result (*start)(uint64_t,uint32_t)) { capture_=capture; allowed_=allowed; start_=start; }
  void begin(autonomous_storage::Ledger*, uint32_t now);
  void observe(const sensors::Reading&);
  void configure(const watering::Config& config);
  void service(bool automatic);
  Outcome manual(uint32_t seconds);
  void interrupt();
  bool maintenance();
  bool takeManualRequest() { const bool r=manualRequested_;manualRequested_=false;return r; }
  const watering::AutonomousDecision& status() const { return view_; }
  uint32_t cooldownRemaining(uint32_t now) const { return ledger_ && ledger_->cooldownRemaining(now)>controller_.cooldownRemaining(now) ? ledger_->cooldownRemaining(now) : controller_.cooldownRemaining(now); }
 private:
  watering::Config config_;
  watering::AutonomousController controller_;
  watering::Controller safety_;
  autonomous_storage::Ledger* ledger_ = nullptr;
  autonomous_storage::Permit permit_;
  watering::AutonomousDecision view_;
  sensors::Reading reading_;
  uint64_t sample_ = 0, fence_ = 0;
  bool manualRequested_ = false;
  bool pollOffFence();
  uint32_t freshness_;
  bool observed_ = false, active_ = false, transaction_ = false, cancelled_ = false;
  uint64_t (*capture_)() = nullptr;
  bool (*allowed_)(uint64_t) = nullptr;
  pump::Result (*start_)(uint64_t,uint32_t) = nullptr;
  void refresh();
  void fault(watering::Block);
  void finish(bool actuatorFault);
  bool renewLease();
  Outcome activate(watering::AutonomousController candidate, const watering::AutonomousDecision&, uint32_t seconds);
};
}
#endif
