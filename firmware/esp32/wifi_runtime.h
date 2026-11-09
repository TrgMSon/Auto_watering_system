#pragma once
#include "wifi_radio.h"
#include "wifi_credentials.h"
namespace wifi_runtime {
enum class State { NoCredentials, Connecting, Online, Backoff, Provisioning, Unavailable };
enum class Reason { None, Missing, Invalid, Timeout, LinkLost, RadioFault, WorkerUnavailable };
struct Snapshot {
  State state=State::Unavailable;
  bool online=false;
  uint32_t ipv4=0; // network byte order; local IPv4, not Internet reachability
  Reason reason=Reason::WorkerUnavailable;
};
// Single worker only. The public service copies Snapshot under its short lock.
class Runtime {
 public:
  Runtime(ble_wifi::EspRadio& radio,ble_wifi::CredentialStorage& storage):radio_(radio),storage_(storage){}
  void boot(uint32_t now){reload(now,false,false);}
  void bootFixed(uint32_t now,const ble_commands::WifiRequest& credentials){
    fixed_=true;
    credentials_=credentials;
    if(!ble_commands::validWifiRequest(credentials_)){
      credentials_.clear();set(State::NoCredentials,Reason::Invalid);return;
    }
    start(now);
  }
  void preempt(){
    if(fixed_)return;
    if(s_.state==State::Connecting||s_.state==State::Online)radio_.stopCandidate();
    set(State::Provisioning);
  }
  void provisioned(uint32_t now,ble_wifi::Persistence outcome){
    if(fixed_)return;
    reload(now,true,outcome==ble_wifi::Persistence::Verified);
  }
  void service(uint32_t now){
    if(s_.state==State::Provisioning||s_.state==State::Unavailable||s_.state==State::NoCredentials)return;
    if(radio_.faulted()){unavailable();return;}
    if(s_.state==State::Backoff){
      if(uint32_t(now-since_)<wait_)return;
      start(now);return;
    }
    if(s_.state==State::Connecting&&uint32_t(now-since_)>=30000){backoff(now,Reason::Timeout);return;}
    const auto link=radio_.poll(); // re-query live association/IP even when Online
    if(radio_.faulted()){unavailable();return;}
    if(link==ble_wifi::Radio::State::CandidateIp){
      level_=0;set(State::Online);s_.online=true;s_.ipv4=radio_.ipv4();
    }else if(s_.state==State::Online||link==ble_wifi::Radio::State::Failed){
      backoff(now,Reason::LinkLost);
    }
  }
  Snapshot snapshot()const{return s_;}
 private:
  void set(State state,Reason reason=Reason::None){s_.state=state;s_.reason=reason;s_.online=false;s_.ipv4=0;}
  void unavailable(){credentials_.clear();set(State::Unavailable,Reason::RadioFault);}
  void start(uint32_t now){
    since_=now;set(State::Connecting);
    if(!radio_.startRam(credentials_)){
      if(radio_.faulted())unavailable();else backoff(now,Reason::LinkLost);
    }
  }
  void backoff(uint32_t now,Reason reason){
    radio_.stopCandidate();
    if(radio_.faulted()){unavailable();return;}
    const uint32_t delays[]={5000,10000,20000,30000};
    wait_=delays[level_];if(level_<3)++level_;
    since_=now;set(State::Backoff,reason);
  }
  void reload(uint32_t now,bool delayed,bool keepLink){
    if(fixed_){
      if(delayed)backoff(now,Reason::LinkLost);else start(now);
      return;
    }
    const auto read=storage_.load(credentials_);
    if(radio_.faulted()){unavailable();return;}
    if(read!=ble_wifi::Read::Ok){
      if(keepLink)radio_.stopCandidate();
      if(radio_.faulted()){unavailable();return;}
      set(State::NoCredentials,read==ble_wifi::Read::Missing?Reason::Missing:Reason::Invalid);return;
    }
    if(keepLink){
      // Verified storage is the source of runtime credentials; no radio restart.
      level_=0;set(State::Connecting);since_=now;service(now);
    }else if(delayed)backoff(now,Reason::LinkLost);
    else start(now);
  }
  ble_wifi::EspRadio& radio_;ble_wifi::CredentialStorage& storage_;
  ble_commands::WifiRequest credentials_;Snapshot s_;
  uint32_t since_=0,wait_=0;unsigned level_=0;
  bool fixed_=false;
};
}
