#pragma once
#include "ble_wifi_request.h"
#include <string.h>
namespace ble_pairing {
enum class Command { None, Open, Close };
inline Command command(const char* line) {
  return !line?Command::None:!strcmp(line,"BLE_PAIR")?Command::Open:
      !strcmp(line,"BLE_PAIR_CLOSE")?Command::Close:Command::None;
}
inline bool ownerAllows(Command command,bool manual,bool pumpOn) {
  return command==Command::Close||(command==Command::Open&&manual&&!pumpOn);
}
class Window {
 public:
  void set(bool open,uint32_t issued) { enabled_=open;issued_=issued; }
  bool open(uint32_t now) const {return enabled_&&uint32_t(now-issued_)<60000;}
 private:
  bool enabled_=false;uint32_t issued_=0;
};
// GAP auth events only carry a peer address, not a GATT connection/procedure ID.
// Retain one procedure's original generation across disconnect, never relabel
// its delayed completion with a new connection generation for the same address.
class Binding {
 public:
  void connect(const uint8_t* peer,uint64_t generation){memcpy(peer_,peer,6);active_=generation;bonded_=false;}
  void disconnect(uint64_t generation){if(active_==generation){active_=0;bonded_=false;}}
  uint64_t capture(const uint8_t* peer)const{return active_&&!memcmp(peer_,peer,6)?active_:0;}
  bool begin(const uint8_t* peer,uint64_t generation){
    if(!generation||capture(peer)!=generation)return false;
    if(inflight_)return inflight_==generation&&!memcmp(procedurePeer_,peer,6);
    inflight_=generation;memcpy(procedurePeer_,peer,6);return true;
  }
  uint64_t complete(const uint8_t* peer){
    if(!inflight_||memcmp(procedurePeer_,peer,6))return 0;
    uint64_t generation=inflight_;inflight_=0;return generation;
  }
  void bonded(uint64_t generation,bool valid){if(active_==generation)bonded_=valid;}
  uint64_t captureWrite(const uint8_t* peer,uint64_t generation)const {
    return bonded_&&generation==active_?capture(peer):0;
  }
  void cancel(uint64_t generation){if(inflight_==generation)inflight_=0;}
 private:
  bool bonded_=false;
  uint8_t peer_[6]={},procedurePeer_[6]={};uint64_t active_=0,inflight_=0;
};
struct Sdk {
  virtual ~Sdk(){}
  virtual bool setup()=0;
  virtual bool bonded(const uint8_t* peer,bool& found)=0;
  virtual bool encrypt(const uint8_t* peer)=0;
  virtual bool respond(const uint8_t* peer,bool accept)=0;
};
class State {
 public:
  bool setup(Sdk& sdk){ready_=sdk.setup();return ready_;}
  bool ready()const{return ready_;}
  void connect(const uint8_t* peer,uint64_t generation){
    memcpy(peer_,peer,6);generation_=generation;admitted_=false;security.connect(generation);
  }
  void disconnect(uint64_t generation){if(generation==generation_){security.disconnect(generation);generation_=0;admitted_=false;}}
  bool matches(const uint8_t* peer,uint64_t generation)const{return generation&&generation_==generation&&!memcmp(peer_,peer,6);}
  bool admit(const uint8_t* peer,uint64_t generation,uint32_t now,Sdk& sdk){
    if(!ready_||!matches(peer,generation))return false;
    bool bond=false;if(!sdk.bonded(peer,bond))return false;
    if(!admitted_&&!bond&&!window.open(now))return false;
    admitted_=true;return true;
  }
  bool authenticated(const uint8_t* peer,uint64_t generation,bool success,bool scBond,Sdk& sdk){
    if(!matches(peer,generation))return false;
    bool bond=false;
    bool valid=ready_&&admitted_&&success&&scBond&&sdk.bonded(peer,bond)&&bond;
    // Public Bluedroid BLE AUTH_CMPL is not an explicit encryption-state API.
    security.update(generation,valid,valid,false);
    if(!valid)admitted_=false;
    return valid;
  }
  Window window;
  ble_commands::WifiSecurity security;
 private:
  bool ready_=false,admitted_=false;uint8_t peer_[6]={};uint64_t generation_=0;
};
}
