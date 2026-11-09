#include "autonomous_owner.h"
#if WATERING_AUTONOMOUS_TEST
#include <Arduino.h>
namespace experimental_watering {
using watering::Block;
bool Owner::pollOffFence() {
  if(capture_ && capture_()!=fence_) {
    interrupt();manualRequested_=true;return true;
  }
  return false;
}
void Owner::refresh() { view_=controller_.evaluate(reading_,millis(),sample_); }
void Owner::fault(Block block) { controller_.latchFault(block,millis()); refresh(); }
void Owner::begin(autonomous_storage::Ledger* ledger,uint32_t now) {
  fence_=capture_?capture_():0;manualRequested_=false;
  ledger_=ledger;active_=transaction_=cancelled_=observed_=false;sample_=0;
  if(!ledger_) { fault(Block::StorageFault); return; }
  const auto loaded=ledger_->load(now);
  const bool restored=ledger_->restoreController(controller_,now);
  view_=watering::AutonomousDecision{};
  if(loaded!=autonomous_storage::Result::Ok||!restored)fault(Block::StorageFault);
  else controller_.latchFault(Block::SensorUnsafe,now); // Fresh 30s safety evidence after every boot.
}
void Owner::observe(const sensors::Reading& r) {
  if(!observed_ || int32_t(r.sampledAtMs-reading_.sampledAtMs)>0)++sample_;
  observed_=true;reading_=r;
}
void Owner::configure(const watering::Config& config) {
  config_=config;controller_.configure(config);safety_.configure(config);freshness_=config.maxReadingAgeMs;
  if(transaction_)cancelled_=true;
}
void Owner::finish(bool actuatorFault) {
  pump::command("OFF");pump::takeStopEvent();
  if(pump::isOn()||pump::remainingMs()!=0){fault(Block::ActuatorFault);return;}
  controller_.finishRun(millis());active_=false;
  transaction_=true;
  const auto completed=ledger_->complete(permit_,true,millis(),actuatorFault);transaction_=false;
  if(completed!=autonomous_storage::Result::Ok){fault(Block::StorageFault);return;}
  if(actuatorFault){fault(Block::ActuatorFault);return;}
  pollOffFence();refresh();
}
bool Owner::renewLease() {
  const bool renewed=pump::renewSafetyLease(reading_.sampledAtMs,freshness_);
  if(renewed && pump::isOn())return true;
  const auto event=pump::takeStopEvent();
  refresh(); // Re-evaluate freshness at the actual rejection time.
  const bool unsafe=safety_.safety(reading_,millis()).reason!=watering::Reason::Idle;
  const bool actuatorFault=event.cause==pump::StopCause::SafetyLeaseExpired ||
      (!unsafe && event.cause!=pump::StopCause::Timeout);
  finish(actuatorFault);return false;
}
void Owner::interrupt() {
  fence_=capture_?capture_():0;
  cancelled_=true;pump::command("OFF");controller_.interrupt(millis());
  if(transaction_)return; // Reservation callback: outer transaction owns finalization.
  if(active_)finish(false);
  if(observed_)refresh();
  else { view_.desiredPumpOn=false;view_.automatic=false;
    if(view_.block!=Block::StorageFault&&view_.block!=Block::ActuatorFault)view_.block=Block::ManualInterruption; }
}
Outcome Owner::activate(watering::AutonomousController candidate,const watering::AutonomousDecision& proposal,uint32_t seconds) {
  if(!ledger_||!capture_||!allowed_||!start_){fault(Block::StorageFault);return Outcome::StorageFault;}
  const uint64_t epoch=capture_();
  if(!allowed_(epoch)){manualRequested_=true;return Outcome::Unsafe;}
  if(safety_.safety(reading_,millis()).reason!=watering::Reason::Idle)return Outcome::Unsafe;
  cancelled_=false;transaction_=true;
  permit_=ledger_->reserve(proposal,ledger_->record().sequence,ledger_->record().sequence+1,millis());
  transaction_=false;
  if(!permit_.allowed()) {
    if(permit_.result==autonomous_storage::Result::Cooldown){view_.state=watering::State::Cooldown;view_.desiredPumpOn=false;return Outcome::Cooldown;}
    if(permit_.result==autonomous_storage::Result::WaitingRetry||permit_.result==autonomous_storage::Result::BudgetExhausted){
      view_.state=watering::State::Locked;view_.desiredPumpOn=false;
      view_.block=permit_.result==autonomous_storage::Result::WaitingRetry?Block::WaitingRetry:Block::BudgetExhausted;return Outcome::Locked;
    }
    fault(Block::StorageFault);return Outcome::StorageFault;
  }
#if WATERING_DEMO_RETRY
  candidate.adoptCommittedRetry(ledger_->committedRetryMs());
#endif
  const auto reconsider=candidate.evaluate(reading_,millis(),sample_);
  if(cancelled_)candidate.configure(config_);
  controller_=candidate; // Accounting is now durable; no GPIO has been enabled yet.
  if(cancelled_||!reconsider.desiredPumpOn||!allowed_(epoch)||safety_.safety(reading_,millis()).reason!=watering::Reason::Idle) {
    if(!allowed_(epoch)){manualRequested_=true;fence_=capture_();}
    controller_.interrupt(millis());active_=true;finish(false);return Outcome::Unsafe;
  }
  const auto result=start_(epoch,seconds);
  if(result==pump::Result::Off) {
    manualRequested_=true;fence_=capture_();
    controller_.interrupt(millis());active_=true;finish(false);return Outcome::Unsafe;
  }
  if(result!=pump::Result::On || !pump::isOn()) {
    active_=true;finish(true);return view_.block==Block::StorageFault?Outcome::StorageFault:Outcome::ActuatorFault;
  }
  const auto run=pump::run();
  if(!controller_.acknowledgeRun(run.startedMs,run.durationMs)) {
    active_=true;finish(true);return view_.block==Block::StorageFault?Outcome::StorageFault:Outcome::ActuatorFault;
  }
  active_=true;refresh();
  if(!view_.desiredPumpOn){finish(false);return Outcome::Unsafe;}
  if(!renewLease())return view_.block==Block::StorageFault?Outcome::StorageFault:
      view_.block==Block::ActuatorFault?Outcome::ActuatorFault:Outcome::Unsafe;
  return Outcome::Started;
}
void Owner::service(bool automatic) {
  if(transaction_)return;
  if(pollOffFence())return;
  controller_.setAutomatic(automatic,millis());
  pump::poll();const auto event=pump::takeStopEvent();
  if(active_) {
    refresh();
    if(!pump::isOn()||!view_.desiredPumpOn) {
      finish(event.cause==pump::StopCause::SafetyLeaseExpired ||
          (event.cause!=pump::StopCause::Timeout && view_.desiredPumpOn));return;
    }
    if(safety_.safety(reading_,millis()).reason==watering::Reason::Idle)renewLease();
    return;
  }
  if(pump::isOn()){
    pump::command("OFF");
    if(!pump::isOn()&&ledger_)ledger_->markMaintenance(true,millis());
    fault(Block::ActuatorFault);return;
  }
  if(!observed_)return;
  auto candidate=controller_;
  const auto proposal=candidate.evaluate(reading_,millis(),sample_);
  if(ledger_ && (ledger_->record().flags&autonomous_storage::Episode) && !proposal.episodeActive &&
      proposal.reservationsTotal==0 && proposal.block==Block::None) {
    transaction_=true;cancelled_=false;
    const auto renewed=ledger_->endEpisode(true,true,millis());transaction_=false;
    if(renewed!=autonomous_storage::Result::Ok){fault(Block::StorageFault);return;}
    if(pollOffFence())cancelled_=true;
    if(cancelled_){candidate.configure(config_);candidate.interrupt(millis());}
  }
  if(proposal.desiredPumpOn) { activate(candidate,proposal,5); return; }
  controller_=candidate;view_=proposal;
}
Outcome Owner::manual(uint32_t seconds) {
  if(pollOffFence())return Outcome::Unsafe;
  if(transaction_||active_||pump::isOn())return Outcome::Busy;
  if(!observed_||!seconds||seconds>5)return Outcome::Unsafe;
  auto candidate=controller_;auto proposal=candidate.requestManual(reading_,millis(),sample_);
  if(!proposal.desiredPumpOn)return proposal.state==watering::State::Cooldown?Outcome::Cooldown:
      proposal.block==Block::SensorUnsafe||proposal.block==Block::WaterUnsafe?Outcome::Unsafe:Outcome::Locked;
  return activate(candidate,proposal,seconds);
}
bool Owner::maintenance() {
  if(transaction_||active_||pump::isOn()||!ledger_)return false;
  transaction_=true;
  const auto initialized=ledger_->maintenanceInitialize(true,millis());transaction_=false;
  if(initialized!=autonomous_storage::Result::Ok){fault(Block::StorageFault);return false;}
  ledger_->restoreController(controller_,millis());controller_.latchFault(Block::SensorUnsafe,millis());refresh();return true;
}
}
#endif
