#include "../esp32/autonomous_storage.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>
using namespace autonomous_storage;
struct Cut {};
struct Memory : Storage {
  std::vector<uint8_t> flash;
  unsigned steps=0, cutAt=0, writes=0;
  bool immediate=false, failWrite=false, failRead=false, corrupt=false, stale=false;
  std::vector<uint8_t> old;
  void step(){if(++steps==cutAt)throw Cut{};}
  Read read(uint8_t* out,size_t cap,size_t& n) override {
    step();n=0;if(failRead)return Read::Error;
    const auto& b=stale?old:flash;if(b.empty())return Read::Missing;
    if(b.size()>cap)return Read::Error;
    n=b.size();memcpy(out,b.data(),n);if(corrupt)out[n-1]^=1;step();return Read::Ok;
  }
  bool writeCommit(const uint8_t* bytes,size_t n) override {
    ++writes;old=flash;step(); // before set
    std::vector<uint8_t> staged(bytes,bytes+n);
    if(immediate)flash=staged;
    step(); // after set; SDK may have written already
    if(failWrite)return false;
    flash=staged;step(); // after commit
    return true;
  }
};
static watering::Config config(){watering::Config c;c.fullDistanceCm=3;c.emptyDistanceCm=12;c.calibrationConfirmed=true;return c;}
static watering::AutonomousDecision proposal(unsigned n) {
  watering::AutonomousDecision p;p.desiredPumpOn=p.episodeActive=true;p.state=watering::State::Watering;
  p.reservationsTotal=n;p.cycle=n<=2?1:2;p.reservationsInCycle=n<=2?n:n-2;return p;
}
static Permit reserve(Ledger& l,unsigned n,uint32_t now) {return l.reserve(proposal(n),l.record().sequence,n,now);}
static unsigned cases=0;
static void pass(){++cases;}
int main(){
 { Memory m;Ledger l(m);assert(l.load(0)==Result::Missing);assert(!reserve(l,1,30000).allowed());
   assert(l.maintenanceInitialize(false,0)==Result::Unsafe);assert(l.maintenanceInitialize(true,0)==Result::Ok);
   assert(reserve(l,1,29999).result==Result::Cooldown);auto p=reserve(l,1,30000);assert(p.allowed());
   assert(l.complete(p,false,35000)==Result::Unsafe);assert(l.complete(p,true,35000)==Result::Ok);
   assert(l.record().total==1&&l.record().flags&Cooldown);pass(); }
 { Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);auto writes=m.writes;
   assert(reserve(l,1,30000).result==Result::Duplicate&&m.writes==writes);
   assert(l.complete(p,true,31000)==Result::Ok);assert(l.complete(p,true,32000)==Result::Conflict);
   assert(reserve(l,2,61000).allowed());pass(); }
 for(bool immediate:{false,true}) {Memory m;m.immediate=immediate;Ledger l(m);l.maintenanceInitialize(true,0);m.failWrite=true;
   assert(!reserve(l,1,30000).allowed());m.failWrite=false;assert(l.load(0)==Result::StorageFault);
   assert(reserve(l,1,60000).result==Result::StorageFault);Ledger reboot(m);
   auto r=reboot.load(0);assert(r==(immediate?Result::MaintenanceRequired:Result::Ok));pass();}
 for(int failure=0;failure<3;++failure){Memory m;Ledger l(m);l.maintenanceInitialize(true,0);
   m.failRead=failure==0;m.corrupt=failure==1;m.stale=failure==2;
   assert(reserve(l,1,30000).result==Result::StorageFault);
   m.failRead=m.corrupt=m.stale=false;assert(reserve(l,1,30000).result==Result::StorageFault);
   Ledger reboot(m);assert(reboot.load(0)==Result::MaintenanceRequired);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);
  Ledger reboot(m);assert(reboot.load(0)==Result::MaintenanceRequired);assert(reboot.record().total==1);
  assert(reboot.complete(p,true,0)==Result::Conflict);watering::AutonomousController c(config());
  assert(reboot.restoreController(c,0));auto r=watering::AutonomousRestore{};(void)r;
  sensors::Reading s;s.sampledAtMs=0;s.soilValid=s.distanceValid=true;s.soilPercent=20;s.distanceCm=4;
  c.setAutomatic(true,0);auto d=c.evaluate(s,0,1);assert(!d.desiredPumpOn&&d.block==watering::Block::StorageFault);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);uint32_t t=30000;
  for(unsigned n=1;n<=4;++n){auto p=reserve(l,n,t);assert(p.allowed());t+=5000;assert(l.complete(p,true,t)==Result::Ok);t+=(n==2?1800000:30000);}
  Ledger reboot(m);assert(reboot.load(0)==Result::Ok);assert(reboot.record().flags&Exhausted);
  assert(reserve(reboot,5,1800000).result==Result::BudgetExhausted);
  assert(reboot.endEpisode(true,false,1800000)==Result::Unsafe);assert(reboot.endEpisode(true,true,1800000)==Result::Ok);
  assert(reboot.record().total==0);assert(reserve(reboot,1,1830000).allowed());pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  Ledger reboot(m);assert(reboot.load(100)==Result::Ok);assert(reserve(reboot,2,30099).result==Result::Cooldown);
  assert(reserve(reboot,2,30100).allowed());pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  p=reserve(l,2,65000);l.complete(p,true,70000);Ledger reboot(m);reboot.load(100);
  assert(reserve(reboot,3,30100).result==Result::WaitingRetry);
  assert(reserve(reboot,3,1800099).result==Result::WaitingRetry);
  assert(reserve(reboot,3,1800100).allowed());pass();}
 for(unsigned byte=0;byte<kRecordSize;++byte){Memory m;Ledger l(m);l.maintenanceInitialize(true,0);m.flash[byte]^=1;
  Ledger reboot(m);assert(reboot.load(0)==Result::Invalid);assert(!reserve(reboot,1,30000).allowed());pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);m.flash[4]=2;auto crc=crc32(m.flash.data(),36);
  for(unsigned i=0;i<4;++i) { m.flash[36+i]=uint8_t(crc>>(8*i)); }
  Ledger reboot(m);assert(reboot.load(0)==Result::Invalid);pass();}
 // Every read/write boundary in reserve, with both staged and immediate persistence.
 for(bool immediate:{false,true})for(unsigned cut=1;cut<=5;++cut){Memory m;m.immediate=immediate;Ledger l(m);l.maintenanceInitialize(true,0);
  m.steps=0;m.cutAt=cut;try{reserve(l,1,30000);assert(false);}catch(Cut&){}
  m.cutAt=0;assert(!reserve(l,1,30000).allowed());Ledger reboot(m);auto r=reboot.load(0);
  assert(r==Result::Ok||r==Result::MaintenanceRequired);
  if(r==Result::MaintenanceRequired)assert(reboot.record().total==1);else assert(reboot.record().total==0);pass();}
 // Interrupted finalization never refunds. No old permit can finalize after reboot.
 for(bool immediate:{false,true})for(unsigned cut=1;cut<=5;++cut){Memory m;m.immediate=immediate;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);
  m.steps=0;m.cutAt=cut;try{l.complete(p,true,35000);assert(false);}catch(Cut&){}
  m.cutAt=0;Ledger reboot(m);auto r=reboot.load(0);assert(reboot.record().total==1);
  assert(r==Result::Ok||r==Result::MaintenanceRequired);assert(reboot.complete(p,true,0)==Result::Conflict);pass();}
 // Renewal and explicit maintenance: crash leaves either old quota or committed new record.
 for(bool maintenance:{false,true})for(bool immediate:{false,true})for(unsigned cut=1;cut<=(maintenance?7u:5u);++cut){
  Memory m;m.immediate=immediate;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  m.steps=0;m.cutAt=cut;try{if(maintenance)l.maintenanceInitialize(true,65000);else l.endEpisode(true,true,65000);assert(false);}catch(Cut&){}
  m.cutAt=0;Ledger reboot(m);assert(reboot.load(0)==Result::Ok);assert(reboot.record().total<=1);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  watering::AutonomousController c(config());assert(l.restoreController(c,123));
  c.setAutomatic(true,123);c.setAutomatic(false,123);c.configure(config());assert(l.record().total==1);
  sensors::Reading s;s.sampledAtMs=123;s.soilValid=s.distanceValid=true;s.soilPercent=70;s.distanceCm=4;
  auto d=c.evaluate(s,123,1);assert(d.reservationsTotal==1&&!d.automatic&&!d.desiredPumpOn);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,UINT32_MAX-10000);auto p=reserve(l,1,19999);assert(p.allowed());pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);assert(l.markMaintenance(true,0)==Result::Ok);
  Ledger reboot(m);assert(reboot.load(0)==Result::MaintenanceRequired);assert(!reserve(reboot,1,30000).allowed());
  assert(reboot.maintenanceInitialize(true,0)==Result::Ok);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);const auto seq=l.record().sequence;
  assert(l.reserve(proposal(1),seq-1,1,30000).result==Result::Conflict);
  assert(l.reserve(proposal(2),seq,1,30000).result==Result::Conflict);assert(!l.record().total);pass();}
 {Memory m;Ledger l(m);assert(l.load(0)==Result::Missing);watering::AutonomousController c(config());assert(!l.restoreController(c,0));pass();}
 {Record r;r.sequence=std::numeric_limits<uint64_t>::max();uint8_t b[kRecordSize];encode(r,b);
  Memory m;m.flash.assign(b,b+kRecordSize);Ledger l(m);assert(l.load(0)==Result::Ok);assert(!reserve(l,1,30000).allowed());pass();}
 assert(crc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);pass();

 for(unsigned length=0;length<kRecordSize;++length){Memory m;Ledger l(m);l.maintenanceInitialize(true,0);m.flash.resize(length);
  Ledger reboot(m);auto result=reboot.load(0);assert(result==(length?Result::Invalid:Result::Missing));pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);watering::AutonomousController c(config());
  assert(l.restoreController(c,0));c.setAutomatic(true,0);auto staged=c;
  sensors::Reading s;s.sampledAtMs=30000;s.soilValid=s.distanceValid=true;s.soilPercent=20;s.distanceCm=4;
  auto d=staged.evaluate(s,30000,1);auto p=l.reserve(d,l.record().sequence,77,30000);assert(p.allowed());
  c=staged;assert(d.reservationsTotal==l.record().total);assert(l.complete(p,true,35000)==Result::Ok);pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  p=reserve(l,2,65000);l.complete(p,true,70000);Ledger reboot(m);reboot.load(100);
  watering::AutonomousController c(config());assert(reboot.restoreController(c,100));c.setAutomatic(true,100);
  sensors::Reading s;s.soilValid=s.distanceValid=true;s.soilPercent=20;s.distanceCm=4;s.sampledAtMs=1800099;
  auto d=c.evaluate(s,s.sampledAtMs,1);assert(!d.desiredPumpOn&&d.block==watering::Block::WaitingRetry);
  s.sampledAtMs=1800100;d=c.evaluate(s,s.sampledAtMs,2);assert(d.desiredPumpOn&&d.reservationsTotal==3);pass();}

 for(unsigned variant=0;variant<7;++variant){Record r;r.sequence=1;
  if(variant==0)r.cycle=1;
  if(variant==1)r.flags=Waiting;
  if(variant==2){r.flags=Episode|Pending|Cooldown;r.total=r.cycle=1;r.request=1;}
  if(variant==3){r.flags=Episode|Cooldown;r.total=2;r.cycle=1;r.request=1;}
  if(variant==4){r.flags=Episode|Cooldown;r.total=4;r.cycle=2;r.request=1;}
  if(variant==5)r.flags=64;
  if(variant==6){r.flags=Episode|Pending;r.total=3;r.cycle=1;r.request=1;}
  uint8_t b[kRecordSize];encode(r,b);Record decoded;assert(!decode(b,sizeof(b),decoded));pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);l.complete(p,true,35000);
  watering::AutonomousController c(config());l.restoreController(c,0);c.setAutomatic(true,0);
  sensors::Reading s;s.soilValid=s.distanceValid=true;s.soilPercent=70;s.distanceCm=4;
  for(unsigned i=0;i<=10;++i){s.sampledAtMs=i*2000;c.evaluate(s,s.sampledAtMs,i+1);}
  // Reboot discards the previous 20s wet evidence; a full fresh interval is required.
  Ledger reboot(m);reboot.load(100);reboot.restoreController(c,100);
  for(unsigned i=0;i<15;++i){s.sampledAtMs=100+i*2000;auto d=c.evaluate(s,s.sampledAtMs,i+1);assert(d.reservationsTotal==1&&!d.automatic);}
  s.sampledAtMs=30100;auto d=c.evaluate(s,s.sampledAtMs,16);assert(d.reservationsTotal==0&&!d.desiredPumpOn);pass();}

 for(bool immediate:{false,true})for(unsigned cut=1;cut<=5;++cut){Memory m;m.immediate=immediate;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);
  m.steps=0;m.cutAt=cut;try{l.complete(p,true,35000,true);assert(false);}catch(Cut&){}
  m.cutAt=0;Ledger reboot(m);assert(reboot.load(0)==Result::MaintenanceRequired);
  assert(reboot.record().total==1&&(reboot.record().flags&(Pending|Maintenance)));pass();}
 {Memory m;Ledger l(m);l.maintenanceInitialize(true,0);auto p=reserve(l,1,30000);assert(l.complete(p,true,35000,true)==Result::Ok);
  Ledger reboot(m);assert(reboot.load(0)==Result::MaintenanceRequired);assert(reboot.record().total==1);pass();}
 std::cout<<"PASS "<<cases<<" B2 persistence scenarios (fake flash, no GPIO/NVS hardware)\n";
}
