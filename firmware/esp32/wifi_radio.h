#pragma once
#include "ble_wifi_attempt.h"
namespace ble_wifi {
// SDK seam. drain() supplies acknowledgement of an explicit event-loop barrier,
// not a generation retroactively attached to arbitrary Wi-Fi callbacks.
struct RadioSdk {
  virtual ~RadioSdk() {}
  virtual bool initialize()=0;
  virtual bool stop()=0;
  virtual bool barrier(uint32_t token)=0;
  virtual bool drain(uint32_t& acknowledged)=0; // false on event overflow/fault
  virtual bool clearIp()=0;
  virtual bool configureRam(const ble_commands::WifiRequest& candidate)=0;
  virtual bool startConnect()=0;
  virtual bool candidateIp(const ble_commands::WifiRequest& candidate)=0;
  // Value copy in network byte order, valid only after candidateIp succeeds.
  virtual uint32_t ipv4() const { return 0; }
};
class EspRadio : public Radio {
 public:
  explicit EspRadio(RadioSdk& sdk):sdk_(sdk){}
  bool startRam(const ble_commands::WifiRequest& r) override {
    if(fault_||(active_&&!completed_)||!ble_commands::validWifiRequest(r))return false;
    if(!initialized_){if(!sdk_.initialize()){fault_=true;return false;}initialized_=true;}
    if(!sdk_.stop()){fault_=true;return false;}
    if(!++token_)++token_;
    candidate_=r;active_=true;waiting_=true;completed_=false;
    if(!sdk_.barrier(token_)){stopCandidate();fault_=true;return false;}
    return true;
  }
  State poll() override {
    if(fault_||!active_)return State::Failed;
    uint32_t ack=0;
    if(!sdk_.drain(ack)){stopCandidate();fault_=true;return State::Failed;}
    if(waiting_){
      if(ack!=token_)return State::Pending;
      if(!sdk_.clearIp()||!sdk_.configureRam(candidate_)||!sdk_.startConnect()){
        stopCandidate();fault_=true;return State::Failed;
      }
      waiting_=false;
    }
    if(sdk_.candidateIp(candidate_)){completed_=true;return State::CandidateIp;}
    return State::Pending;
  }
  void stopCandidate() override {
    if(initialized_&&!sdk_.stop())fault_=true;
    candidate_.clear();active_=waiting_=completed_=false;
  }
  bool faulted() const{return fault_;}
  uint32_t ipv4() const{return sdk_.ipv4();}
 private:
  RadioSdk& sdk_;ble_commands::WifiRequest candidate_;
  uint32_t token_=0;bool initialized_=false,active_=false,waiting_=false,completed_=false,fault_=false;
};
}
