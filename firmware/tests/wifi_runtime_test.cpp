#include "../esp32/wifi_runtime.h"
#include <cassert>
#include <iostream>
using namespace ble_wifi;
using namespace wifi_runtime;
struct Sdk:RadioSdk {
  unsigned init=0,starts=0,stops=0;uint32_t token=0;bool ip=false,fail=false;
  char configured[33]={};
  bool initialize()override{++init;return !fail;}
  bool stop()override{++stops;ip=false;return !fail;}
  bool barrier(uint32_t t)override{token=t;return !fail;}
  bool drain(uint32_t& t)override{t=token;return !fail;}
  bool clearIp()override{return true;}
  bool configureRam(const ble_commands::WifiRequest& r)override{strcpy(configured,r.ssid);return true;}
  bool startConnect()override{++starts;return true;}
  bool candidateIp(const ble_commands::WifiRequest&)override{return ip;}
  uint32_t ipv4()const override{return ip?0x0201a8c0:0;}
};
struct Memory:Blob {
  uint8_t data[kCredentialMax]={};size_t n=0;unsigned reads=0,writes=0;Read status=Read::Missing;
  Read read(uint8_t* out,size_t,size_t& size)override{++reads;size=n;memcpy(out,data,n);return status;}
  Write writeCommit(const uint8_t* p,size_t size)override{++writes;n=size;memcpy(data,p,n);status=Read::Ok;return Write::Committed;}
};
const uint8_t mac[]={1,2,3,4,5,6};
ble_commands::WifiRequest request(){ble_commands::WifiRequest r;strcpy(r.ssid,"fixture");strcpy(r.password,"fixture-pass");return r;}
struct Fixture {
  Sdk sdk;EspRadio radio{sdk};Memory blob;CredentialStorage store{blob,mac};Runtime runtime{radio,store};
  void valid(){auto r=request();blob.n=encodeCredentials(r,mac,blob.data);blob.status=Read::Ok;}
};
int main(){
  for(bool invalid:{false,true}){
    Fixture f;if(invalid){f.blob.status=Read::Ok;f.blob.n=24;}
    f.runtime.boot(0);f.runtime.service(100000);
    assert(f.runtime.snapshot().state==State::NoCredentials&&!f.sdk.init&&!f.blob.writes);
    assert(f.runtime.snapshot().reason==(invalid?Reason::Invalid:Reason::Missing));
  }
  for(uint32_t origin:{0u,UINT32_MAX-1000u}){
    Fixture f;f.valid();uint32_t now=origin;f.runtime.boot(now);f.runtime.service(now);
    assert(f.sdk.starts==1&&f.runtime.snapshot().state==State::Connecting);
    for(uint32_t wait:{5000u,10000u,20000u,30000u,30000u}){
      now+=30000;f.runtime.service(now);assert(f.runtime.snapshot().reason==Reason::Timeout);
      unsigned count=f.sdk.starts;f.runtime.service(now+wait-1);assert(f.sdk.starts==count);
      now+=wait;f.runtime.service(now);f.runtime.service(now);assert(f.sdk.starts==count+1);
    }
    f.sdk.ip=true;f.runtime.service(++now);auto copy=f.runtime.snapshot();
    assert(copy.online&&copy.ipv4==0x0201a8c0&&copy.state==State::Online);
    f.sdk.ip=false;f.runtime.service(++now);assert(!f.runtime.snapshot().online&&copy.online);
    unsigned count=f.sdk.starts;f.runtime.service(now+4999);assert(f.sdk.starts==count);
    now+=5000;f.runtime.service(now);f.runtime.service(now);assert(f.sdk.starts==count+1);
    assert(f.blob.writes==0&&f.blob.reads==1&&f.sdk.init==1);
  }
  // Provisioning owns the same radio after preemption; runtime cannot call it.
  for(auto outcome:{Persistence::Verified,Persistence::NotCommitted,Persistence::Unconfirmed}){
    Fixture f;f.valid();f.runtime.boot(0);f.runtime.service(0);f.runtime.preempt();
    unsigned count=f.sdk.starts;f.runtime.service(90000);assert(f.sdk.starts==count);
    assert(f.runtime.snapshot().state==State::Provisioning);
    auto candidate=request();strcpy(candidate.ssid,"new-fixture");
    assert(f.radio.startRam(candidate));f.radio.poll();f.sdk.ip=true;
    assert(f.radio.poll()==Radio::State::CandidateIp);
    if(outcome==Persistence::Verified)assert(f.store.commitVerified(candidate)==outcome);
    else {
      f.radio.stopCandidate();
      // An unconfirmed write can leave the NEW record, not just the old one.
      if(outcome==Persistence::Unconfirmed)f.blob.n=encodeCredentials(candidate,mac,f.blob.data);
    }
    unsigned stops=f.sdk.stops;count=f.sdk.starts;unsigned reads=f.blob.reads;
    f.runtime.provisioned(90001,outcome);
    assert(f.blob.reads==reads+1);
    if(outcome==Persistence::Verified){assert(f.runtime.snapshot().online&&f.sdk.stops==stops&&f.sdk.starts==count);}
    else {
      assert(f.runtime.snapshot().state==State::Backoff);f.runtime.service(95001);f.runtime.service(95001);
      assert(f.sdk.starts==count+1);
      assert(!strcmp(f.sdk.configured,outcome==Persistence::Unconfirmed?"new-fixture":"fixture"));
    }
  }
  for(bool invalid:{false,true}){
    Fixture f;f.valid();f.runtime.boot(0);f.runtime.preempt();
    f.blob.status=invalid?Read::Error:Read::Missing;
    f.runtime.provisioned(1,Persistence::Unconfirmed);
    assert(f.runtime.snapshot().state==State::NoCredentials&&!f.runtime.snapshot().online);
  }
  // Actual Attempt may reset to Idle on BLE disconnect after committing.
  // Its persistence outcome must still drive runtime adoption, without a result
  // being delivered to another generation.
  for(bool disconnected:{false,true}){
    Fixture f;f.valid();f.runtime.boot(0);f.runtime.service(0);f.sdk.ip=true;f.runtime.service(1);
    assert(f.runtime.snapshot().online);f.runtime.preempt();assert(!f.sdk.ip);
    ble_commands::WifiAuthentication auth;auth.generation=7;auth.encryptedWriteGeneration=7;
    auth.code=ble_commands::Code::Ok;auth.request=request();
    ble_commands::WifiSecurity security;security.connect(7);security.update(7,true,true,false);
    Attempt attempt;assert(attempt.reserve(7,auth,true,false,185,2));
    attempt.service(2,7,f.radio,f.store,security);f.sdk.ip=true;
    attempt.service(3,disconnected?8:7,f.radio,f.store,security);
    assert(attempt.persistence()==Persistence::Verified);
    ble_commands::Code code;
    assert(attempt.result(disconnected?8:7,3,code)==!disconnected);
    unsigned count=f.sdk.starts,stops=f.sdk.stops;
    f.runtime.provisioned(3,attempt.persistence());f.runtime.service(4);
    assert(f.runtime.snapshot().online&&f.sdk.starts==count&&f.sdk.stops==stops&&f.blob.writes==1);
  }
  for(bool atBoot:{false,true}){
    Fixture f;f.valid();f.sdk.fail=atBoot;f.runtime.boot(0);
    if(!atBoot){f.runtime.service(0);f.sdk.fail=true;f.runtime.service(1);}
    assert(f.runtime.snapshot().state==State::Unavailable);
    unsigned count=f.sdk.init;for(unsigned i=0;i<100;++i)f.runtime.service(i*30000);
    assert(f.sdk.init==count&&count==1&&!f.blob.writes);
  }
  {
    Fixture f;auto fixed=request();strcpy(fixed.ssid,"fixed-network");
    f.runtime.bootFixed(0,fixed);
    assert(f.sdk.init==1&&f.sdk.starts==0&&f.blob.reads==0&&f.blob.writes==0);
    f.runtime.service(1);
    assert(f.sdk.starts==1&&!strcmp(f.sdk.configured,"fixed-network"));
    f.sdk.ip=true;f.runtime.service(2);assert(f.runtime.snapshot().online);
    f.sdk.ip=false;f.runtime.service(3);assert(f.runtime.snapshot().state==State::Backoff);
    f.runtime.preempt();f.runtime.provisioned(4,Persistence::Verified);
    assert(f.runtime.snapshot().state==State::Backoff&&f.blob.reads==0&&f.blob.writes==0);
    f.runtime.service(5002);assert(f.sdk.starts==1);
    f.runtime.service(5003);f.runtime.service(5004);
    assert(f.sdk.starts==2&&!strcmp(f.sdk.configured,"fixed-network"));
    assert(f.blob.reads==0&&f.blob.writes==0);
  }
  {
    Fixture f;auto invalid=request();invalid.ssid[0]=0;
    f.runtime.bootFixed(0,invalid);
    assert(f.runtime.snapshot().state==State::NoCredentials&&f.sdk.init==0&&f.blob.reads==0);
  }
  std::cout<<"PASS Wi-Fi runtime: persisted and fixed boot, timeout/rollover/backoff/reset, live IP, preemption, persistence reload, latched faults\n";
}
