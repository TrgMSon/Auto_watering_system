#include "pump/Arduino.h"
#include "../esp32/autonomous_owner.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
uint32_t fakeNow=0;int outputLevel=-1,writes=0,outputModes=0;
static bool on=false,rejectLease=false,ageDuringLease=false;
static pump::Run activeRun;
namespace pump {
bool isOn(){return on;}
uint32_t remainingMs(){return on?activeRun.durationMs:0;}
Run run(){return activeRun;}
bool poll(){return false;}
StopEvent takeStopEvent(){return StopEvent{};}
Result command(const char* s){assert(!strcmp(s,"OFF"));on=false;return Result::Off;}
bool renewSafetyLease(uint32_t,uint32_t){
 if(ageDuringLease){fakeNow+=4001;return false;}
 return !rejectLease;
}
}
static uint64_t capture(){return 0;}
static bool allowed(uint64_t){return true;}
static pump::Result start(uint64_t,uint32_t seconds){
 assert(!on&&seconds<=5);on=true;activeRun.startedMs=fakeNow;activeRun.durationMs=seconds*1000;return pump::Result::On;
}
struct Memory:autonomous_storage::Storage {
 std::vector<uint8_t> bytes;
 autonomous_storage::Read read(uint8_t* p,size_t cap,size_t& n)override{
  assert(!on);n=bytes.size();if(!n)return autonomous_storage::Read::Missing;
  assert(cap>=n);memcpy(p,bytes.data(),n);return autonomous_storage::Read::Ok;
 }
 bool writeCommit(const uint8_t* p,size_t n)override{assert(!on);bytes.assign(p,p+n);return true;}
};
int main(){
 unsigned cases=0;
 for(bool active:{false,true})for(bool stale:{false,true}){
  fakeNow=0;on=rejectLease=ageDuringLease=false;Memory mem;autonomous_storage::Ledger ledger(mem);
  assert(ledger.maintenanceInitialize(true,0)==autonomous_storage::Result::Ok);
  watering::Config config;config.fullDistanceCm=3;config.emptyDistanceCm=12;config.calibrationConfirmed=true;
  experimental_watering::Owner owner(config);owner.setGate(capture,allowed,start);owner.begin(&ledger,0);
  sensors::Reading r;r.soilValid=r.distanceValid=true;r.soilPercent=20;r.distanceCm=4;
  for(unsigned i=0;i<=15;++i){fakeNow=i*2000;r.sampledAtMs=fakeNow;owner.observe(r);owner.service(false);}
  if(active)assert(owner.manual(5)==experimental_watering::Outcome::Started);
  rejectLease=!stale;ageDuringLease=stale;
  if(active)owner.service(false);
  else assert(owner.manual(5)!=experimental_watering::Outcome::Started);
  assert(!on&&ledger.record().total==1&&!(ledger.record().flags&autonomous_storage::Pending));
  assert(owner.status().block==(stale?watering::Block::SensorUnsafe:watering::Block::ActuatorFault));
  assert(!owner.status().desiredPumpOn);++cases;
 }
 std::cout<<"PASS "<<cases<<" lease rejection scenarios (fake actuator, no hardware)\n";
}
