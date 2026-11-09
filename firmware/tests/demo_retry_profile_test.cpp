#include "../esp32/autonomous_storage.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>
using namespace autonomous_storage;
struct Memory : Storage {
 std::vector<uint8_t> bytes; bool failRead=false,failWrite=false,corrupt=false;
 std::function<void()> during;
 Read read(uint8_t* p,size_t cap,size_t& n) override {
  n=0;if(failRead)return Read::Error;if(bytes.empty())return Read::Missing;
  assert(cap>=bytes.size());n=bytes.size();memcpy(p,bytes.data(),n);if(corrupt)p[n-1]^=1;return Read::Ok;
 }
 bool writeCommit(const uint8_t* p,size_t n) override {
  bytes.assign(p,p+n);if(during){auto f=during;during=nullptr;f();}return !failWrite;
 }
 void seed(unsigned total=0,uint32_t extra=0) {
  Record r;r.sequence=10;r.total=total;r.flags=Cooldown|extra;
  if(total){r.cycle=total<=2?1:2;r.request=total;r.flags|=Episode;
   if(total==2)r.flags|=Waiting;
   if(total==4)r.flags|=Exhausted;}
  uint8_t b[kRecordSize];encode(r,b);bytes.assign(b,b+sizeof(b));
 }
};
static watering::AutonomousDecision proposal(unsigned n){
 watering::AutonomousDecision p;p.desiredPumpOn=p.episodeActive=true;p.state=watering::State::Watering;
 p.reservationsTotal=n;p.cycle=n<=2?1:2;p.reservationsInCycle=n<=2?n:n-2;return p;
}
static Permit reserve(Ledger& l,unsigned n,uint32_t t){return l.reserve(proposal(n),l.record().sequence,l.record().sequence+1,t);}
static void duration(Ledger& l,uint32_t expected){
#if WATERING_DEMO_RETRY
 assert(l.committedRetryMs()==expected);
#else
 (void)l;assert(expected==1800000);
#endif
}
static uint32_t selected(){return WATERING_DEMO_RETRY?60000u:1800000u;}
static watering::Config config(){watering::Config c;c.fullDistanceCm=3;c.emptyDistanceCm=12;c.calibrationConfirmed=true;return c;}
static sensors::Reading reading(uint32_t t,float soil=20){sensors::Reading r;r.sampledAtMs=t;r.soilValid=r.distanceValid=true;r.soilPercent=soil;r.distanceCm=4;return r;}
int main(){
 // First reservation is the commit point, never load/proposal/set/failed commit.
 {Memory m;m.seed();Ledger l(m);assert(l.load(0)==Result::Ok);duration(l,1800000);
  assert(reserve(l,1,29999).result==Result::Cooldown);duration(l,1800000);
  m.during=[&]{duration(l,1800000);};auto p=reserve(l,1,30000);assert(p.allowed());duration(l,selected());
  assert(l.complete(p,true,35000)==Result::Ok);assert(reserve(l,2,64999).result==Result::Cooldown);
  p=reserve(l,2,65000);assert(p.allowed());assert(l.complete(p,true,70000)==Result::Ok);
  const uint32_t wait=selected();assert(reserve(l,3,70000+wait-1).result==Result::WaitingRetry);
  // Even repeating load cannot refill eligibility or reset retry timestamps.
  assert(l.load(70000+wait-1)==Result::Ok);
  p=reserve(l,3,70000+wait);assert(p.allowed()&&l.record().cycle==2);
  assert(l.complete(p,true,75000+wait)==Result::Ok);p=reserve(l,4,105000+wait);assert(p.allowed());
  assert(l.complete(p,true,110000+wait)==Result::Ok);
  assert(reserve(l,5,140000+wait).result==Result::BudgetExhausted);
  // Legitimate episode completion does not rearm DEMO in the same boot.
  assert(l.endEpisode(true,true,140000+wait)==Result::Ok);duration(l,1800000);
  p=reserve(l,1,170000+wait);assert(p.allowed());duration(l,1800000);
 }
 // Every restored active episode is production, including before retry begins.
 for(unsigned n=1;n<=4;++n){Memory m;m.seed(n);Ledger l(m);assert(l.load(100)==Result::Ok);duration(l,1800000);
  if(n==1){auto p=reserve(l,2,30100);assert(p.allowed());l.complete(p,true,35100);
   assert(reserve(l,3,95100).result==Result::WaitingRetry);}
  if(n==2){assert(reserve(l,3,60100).result==Result::WaitingRetry);
   assert(reserve(l,3,1800099).result==Result::WaitingRetry);assert(reserve(l,3,1800100).allowed());}
  if(n==4)assert(reserve(l,5,1800100).result==Result::BudgetExhausted);
 }
 for(uint32_t flag:{uint32_t(Pending),uint32_t(Maintenance)}){
  Memory m;m.seed(flag==Pending?1:0,flag);
  if(flag==Pending){Record r;r.sequence=1;r.request=1;r.total=r.cycle=1;r.flags=Episode|Pending;uint8_t b[kRecordSize];encode(r,b);m.bytes.assign(b,b+sizeof(b));}
  Ledger l(m);assert(l.load(0)==Result::MaintenanceRequired);assert(!reserve(l,1,1800000).allowed());duration(l,1800000);
 }
 // Missing, invalid, read/commit/readback failure stay closed.
 for(unsigned fault=0;fault<6;++fault){Memory m;if(fault!=0)m.seed();
  m.failRead=fault==1;m.corrupt=fault==2;Ledger l(m);auto loaded=l.load(0);
  if(fault<3){assert(loaded!=Result::Ok);assert(!reserve(l,1,30000).allowed());}
  else {assert(loaded==Result::Ok);m.failWrite=fault==3;
   if(fault==4)m.during=[&]{m.failRead=true;};
   if(fault==5)m.during=[&]{m.corrupt=true;};
   assert(!reserve(l,1,30000).allowed());duration(l,1800000);
   m.failWrite=m.failRead=m.corrupt=false;assert(!reserve(l,1,1800000).allowed());}
 }
 // Maintenance never awards eligibility, even if it precedes the first load.
 for(bool loadFirst:{false,true}){Memory m;m.seed();Ledger l(m);if(loadFirst)assert(l.load(0)==Result::Ok);
  assert(l.maintenanceInitialize(true,0)==Result::Ok);assert(l.load(0)==Result::Ok);
  auto p=reserve(l,1,30000);assert(p.allowed());duration(l,1800000);
 }
 // DEMO wait is deliberately forgotten on reboot, serialized layout unchanged.
 {Memory m;m.seed();Ledger l(m);l.load(0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  p=reserve(l,2,65000);l.complete(p,true,70000);assert(m.bytes.size()==40);
  Record r;assert(decode(m.bytes.data(),m.bytes.size(),r)&&r.total==2&&(r.flags&Waiting));
  Ledger reboot(m);assert(reboot.load(0)==Result::Ok);duration(reboot,1800000);
  assert(reserve(reboot,3,60000).result==Result::WaitingRetry);
 }
 // Domain timing: continuous acquisitions, unchanged 30s cooldown/wet evidence.
 {watering::AutonomousController c(config());watering::AutonomousRestore r;r.initialized=true;
  assert(c.restore(r,0));c.setAutomatic(true,0);auto d=c.evaluate(reading(0),0,1);assert(d.desiredPumpOn);
#if WATERING_DEMO_RETRY
  c.adoptCommittedRetry(selected());
#endif
  c.finishRun(5000);uint64_t id=1;
  for(uint32_t t=6000;t<35000;t+=1000){d=c.evaluate(reading(t),t,++id);assert(!d.desiredPumpOn);}
  d=c.evaluate(reading(35000),35000,++id);assert(d.desiredPumpOn&&d.reservationsTotal==2);c.finishRun(40000);
  for(uint32_t t=41000;t<40000+selected();t+=1000){d=c.evaluate(reading(t),t,++id);assert(!d.desiredPumpOn);}
  d=c.evaluate(reading(40000+selected()-1),40000+selected()-1,++id);assert(!d.desiredPumpOn);
  d=c.evaluate(reading(40000+selected()),40000+selected(),++id);assert(d.desiredPumpOn&&d.cycle==2);
  c.finishRun(45000+selected());
  for(uint32_t dt=0;dt<30000;dt+=1000){const uint32_t t=45000+selected()+dt;d=c.evaluate(reading(t,80),t,++id);assert(d.reservationsTotal==3);}
  const uint32_t t=75000+selected();d=c.evaluate(reading(t,80),t,++id);assert(d.reservationsTotal==0&&!d.episodeActive);
 }
 std::cout<<"PASS retry profile demo="<<WATERING_DEMO_RETRY<<"\n";
}
