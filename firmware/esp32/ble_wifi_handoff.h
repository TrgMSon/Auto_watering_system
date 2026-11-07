#pragma once
#include "ble_wifi_attempt.h"
#include <stdio.h>
namespace ble_wifi {
constexpr size_t kWriteCapacity=182; // local ATT MTU target/maximum 185 minus 3
constexpr size_t kBusinessMax=sizeof("{\"op\":\"WIFI\",\"code\":\"NONCE_EXPIRED\"}")-1;
// One bounded ingress buffer; the existing FreeRTOS event queue carries only
// its ticket, so popped queue storage never retains credential payload copies.
class WriteBuffer {
 public:
  ~WriteBuffer(){clear();}
  uint32_t store(uint64_t generation,const char* bytes,size_t size){
    if(occupied_||!generation||!size||size>sizeof(bytes_))return 0;
    if(!++serial_)++serial_;
    memcpy(bytes_,bytes,size);size_=size;generation_=generation;occupied_=true;return serial_;
  }
  bool take(uint32_t ticket,uint64_t generation,char* out,size_t capacity,size_t& size){
    size=0;
    if(!occupied_||!ticket||ticket!=serial_||generation!=generation_)return false;
    if(capacity<size_){clear();return false;}
    memcpy(out,bytes_,size_);size=size_;clear();return true;
  }
  void clear(){ble_commands::wipeWifi(bytes_,sizeof(bytes_));size_=0;generation_=0;occupied_=false;}
 private:
  char bytes_[kWriteCapacity]={};size_t size_=0;uint64_t generation_=0;uint32_t serial_=0;bool occupied_=false;
};
// Caller serializes short transitions. Never hold the lock while invoking SDK,
// crypto, logging, radio/storage or Attempt::service.
class Handoff {
 public:
  enum class State { Free, Pending, Working, Done };
  void connection(uint64_t current){
    current_=current;
    if(state_!=State::Working&&generation_!=current_)clear();
  }
  bool reserve(const ble_commands::WifiAuthentication& auth,uint32_t now){
    if(state_!=State::Free||!current_||auth.generation!=current_||auth.securityDenied||auth.transportDenied||auth.missingKey)return false;
    generation_=current_;queued_=now;
    if(auth.code!=ble_commands::Code::Ok){code_=auth.code;finished_=now;state_=State::Done;}
    else {request_=auth;state_=State::Pending;}
    return true;
  }
  bool take(uint32_t now,ble_commands::WifiAuthentication& out){
    if(state_!=State::Pending)return false;
    if(generation_!=current_){clear();return false;}
    if(uint32_t(now-queued_)>=1000){code_=ble_commands::Code::WifiFail;finished_=now;request_.request.clear();state_=State::Done;return false;}
    out=request_;request_.request.clear();state_=State::Working;return true;
  }
  void complete(uint64_t generation,ble_commands::Code code,uint32_t now){
    if(state_!=State::Working||generation!=generation_)return;
    if(generation!=current_){clear();return;}
    code_=code;finished_=now;state_=State::Done;
  }
  bool result(uint32_t now,uint64_t& generation,ble_commands::Code& code){
    if(state_!=State::Done)return false;
    if(generation_!=current_||uint32_t(now-finished_)>=3000){clear();return false;}
    generation=generation_;code=code_;return true;
  }
  void release(uint64_t generation){if(state_==State::Done&&generation_==generation)clear();}
  uint64_t current()const{return current_;}
  State state()const{return state_;}
 private:
  void clear(){request_.request.clear();state_=State::Free;generation_=0;code_=ble_commands::Code::None;}
  State state_=State::Free;uint64_t current_=0,generation_=0;
  uint32_t queued_=0,finished_=0;ble_commands::WifiAuthentication request_;ble_commands::Code code_=ble_commands::Code::None;
};
inline size_t wifiResult(ble_commands::Code code,char* out,size_t capacity){
  if(!out||!capacity)return 0;
  out[0]=0;
  using C=ble_commands::Code;
  if(code!=C::WifiOk&&code!=C::WifiFail&&code!=C::BadSig&&code!=C::BadJson&&code!=C::NonceExpired&&code!=C::OutOfRange)return 0;
  int n=snprintf(out,capacity,"{\"op\":\"WIFI\",\"code\":\"%s\"}",ble_commands::name(code));
  if(n<=0||size_t(n)>=capacity){out[0]=0;return 0;}return size_t(n);
}
}
