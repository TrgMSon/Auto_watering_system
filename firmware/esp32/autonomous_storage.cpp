#include "autonomous_storage.h"
#include <string.h>
#include <limits>
namespace autonomous_storage {
namespace {
void put32(uint8_t* p, uint32_t n) { for (unsigned i=0;i<4;++i) p[i]=uint8_t(n>>(8*i)); }
uint32_t get32(const uint8_t* p) { uint32_t n=0; for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(8*i); return n; }
void put64(uint8_t* p,uint64_t n) { put32(p,uint32_t(n)); put32(p+4,uint32_t(n>>32)); }
uint64_t get64(const uint8_t* p) { return get32(p)|(uint64_t(get32(p+4))<<32); }
}
uint32_t crc32(const uint8_t* p,size_t n) {
  uint32_t crc=0xffffffffu;
  for(size_t i=0;i<n;++i) { crc^=p[i]; for(unsigned b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
  return ~crc;
}
bool valid(const Record& r) {
  if (!r.sequence || r.flags & ~uint32_t(63) || r.total>4) return false;
  if (!r.total) return !r.cycle && !(r.flags & (Episode|Pending|Waiting|Exhausted));
  if (!(r.flags&Episode) || !r.request || r.cycle!=(r.total<=2?1u:2u)) return false;
  if ((r.flags&Pending) && (r.flags&(Cooldown|Waiting|Exhausted))) return false;
  if (!(r.flags&Pending) && !(r.flags&Cooldown)) return false;
  if (bool(r.flags&Waiting) != (r.total==2 && !(r.flags&Pending))) return false;
  return bool(r.flags&Exhausted)==(r.total==4 && !(r.flags&Pending));
}
void encode(const Record& r,uint8_t (&b)[kRecordSize]) {
  put32(b,0x32425741); put32(b+4,1); put64(b+8,r.sequence);
  put32(b+16,r.flags); put32(b+20,r.cycle); put32(b+24,r.total);
  put64(b+28,r.request); put32(b+36,crc32(b,36));
}
bool decode(const uint8_t* b,size_t n,Record& out) {
  if(!b||n!=kRecordSize||get32(b)!=0x32425741||get32(b+4)!=1||get32(b+36)!=crc32(b,36))return false;
  Record r; r.sequence=get64(b+8);r.flags=get32(b+16);r.cycle=get32(b+20);
  r.total=get32(b+24);r.request=get64(b+28);if(!valid(r))return false;out=r;return true;
}
Result Ledger::gate() const {
  if(!loaded_||fault_)return Result::StorageFault;
  if(record_.flags&(Pending|Maintenance))return Result::MaintenanceRequired;
  return Result::Ok;
}
Result Ledger::load(uint32_t now) {
  if(attemptedLoad_)return gate(); // Failed sessions cannot heal through reload.
  attemptedLoad_=true;uint8_t b[kRecordSize];size_t n=0;
  const Read read=storage_.read(b,sizeof(b),n);
  if(read!=Read::Ok){fault_=true;return read==Read::Missing?Result::Missing:Result::StorageFault;}
  if(!decode(b,n,record_)){fault_=true;return Result::Invalid;}
  loaded_=true;cooldownAt_=retryAt_=now;
#if WATERING_DEMO_RETRY
  demoEligible_ = record_.total == 0 && record_.cycle == 0 &&
      !(record_.flags & (Episode|Pending|Maintenance|Waiting|Exhausted));
#endif
  return gate();
}
Result Ledger::commit(Record candidate) {
  if(record_.sequence==std::numeric_limits<uint64_t>::max()){fault_=true;return Result::StorageFault;}
  candidate.sequence=record_.sequence+1;
  if(!valid(candidate)){fault_=true;return Result::StorageFault;}
  uint8_t bytes[kRecordSize],back[kRecordSize];encode(candidate,bytes);size_t n=0;Record verified;
  // Latch BEFORE I/O, including exceptions/power-cut simulations. No permit yet.
  fault_=true;
  if(!storage_.writeCommit(bytes,sizeof(bytes))||storage_.read(back,sizeof(back),n)!=Read::Ok||
      n!=sizeof(bytes)||memcmp(bytes,back,n)||!decode(back,n,verified))return Result::StorageFault;
  record_=verified;loaded_=true;fault_=false;attemptedLoad_=true;return Result::Ok;
}
Result Ledger::maintenanceInitialize(bool off,uint32_t now) {
#if WATERING_DEMO_RETRY
  demoEligible_ = false; episodeRetryMs_ = 1800000;
  // Maintenance must not be followed by load() to manufacture boot eligibility.
  attemptedLoad_ = true;
#endif
  if(!off)return Result::Unsafe;
  // Read a trustworthy sequence if available, even when recovering a fresh session.
  uint8_t b[kRecordSize];size_t n=0;Record old;
  if(storage_.read(b,sizeof(b),n)==Read::Ok&&decode(b,n,old))record_=old;
  issued_=false;
  Record fresh;fresh.flags=Cooldown;
  const auto result=commit(fresh);if(result==Result::Ok)cooldownAt_=retryAt_=now;
  return result;
}
Permit Ledger::reserve(const watering::AutonomousDecision& p,uint64_t expected,uint64_t request,uint32_t now) {
  Permit permit;
  if(fault_){permit.result=Result::StorageFault;return permit;}
  if(loaded_ && request && request==record_.request){permit.result=Result::Duplicate;return permit;}
  permit.result=gate();if(permit.result!=Result::Ok)return permit;
  if(expected!=record_.sequence||!request){permit.result=Result::Conflict;return permit;}
  if(record_.flags&Exhausted){permit.result=Result::BudgetExhausted;return permit;}
  if((record_.flags&Cooldown)&&uint32_t(now-cooldownAt_)<30000){permit.result=Result::Cooldown;return permit;}
#if WATERING_DEMO_RETRY
  if((record_.flags&Waiting)&&uint32_t(now-retryAt_)<episodeRetryMs_){permit.result=Result::WaitingRetry;return permit;}
#else
  if((record_.flags&Waiting)&&uint32_t(now-retryAt_)<1800000){permit.result=Result::WaitingRetry;return permit;}
#endif
  const unsigned next=record_.total+1, cycle=next<=2?1:2;
  if(!p.desiredPumpOn||p.state!=watering::State::Watering||!p.episodeActive||
      p.block!=watering::Block::None||p.reservationsTotal!=next||p.cycle!=cycle||
      p.reservationsInCycle!=(cycle==1?next:next-2)){permit.result=Result::Conflict;return permit;}
  Record candidate=record_;candidate.flags=Episode|Pending;candidate.total=next;
  candidate.cycle=cycle;candidate.request=request;
#if WATERING_DEMO_RETRY
  const bool grantDemo = demoEligible_ && record_.total == 0;
  demoEligible_ = false; // Consume once, including a failed/ambiguous commit.
#endif
  permit.result=commit(candidate);
#if WATERING_DEMO_RETRY
  if(permit.allowed() && next == 1) episodeRetryMs_ = grantDemo ? 60000 : 1800000;
#endif
  if(permit.allowed()){issued_=true;permit.sequence=record_.sequence;permit.request=request;}
  return permit;
}
Result Ledger::complete(const Permit& permit,bool off,uint32_t now,bool maintenanceRequired) {
  if(!off)return Result::Unsafe;
  if(!loaded_||fault_)return Result::StorageFault;
  if(!issued_||!permit.allowed()||!(record_.flags&Pending)||record_.flags&Maintenance||
      permit.sequence!=record_.sequence||permit.request!=record_.request)return Result::Conflict;
  issued_=false;
  Record candidate=record_;candidate.flags=Episode|Cooldown;
  if(maintenanceRequired)candidate.flags|=Maintenance;
  if(candidate.total==2)candidate.flags|=Waiting;
  if(candidate.total==4)candidate.flags|=Exhausted;
  const auto result=commit(candidate);
  if(result==Result::Ok)cooldownAt_=retryAt_=now;
  return result;
}
Result Ledger::endEpisode(bool off,bool wet,uint32_t now) {
  if(!off||!wet)return Result::Unsafe;
  const auto ready=gate();if(ready!=Result::Ok)return ready;
  if((record_.flags&Cooldown)&&uint32_t(now-cooldownAt_)<30000)return Result::Cooldown;
  if(!(record_.flags&Episode))return Result::Conflict; // No repeated renewal writes.
  Record candidate;candidate.flags=Cooldown;candidate.request=record_.request;
  const auto result=commit(candidate);if(result==Result::Ok)cooldownAt_=retryAt_=now;
#if WATERING_DEMO_RETRY
  demoEligible_ = false; episodeRetryMs_ = 1800000;
#endif
  return result;
}
Result Ledger::markMaintenance(bool off,uint32_t) {
#if WATERING_DEMO_RETRY
  demoEligible_ = false;
#endif
  if(!off)return Result::Unsafe;
  if(!loaded_||fault_)return Result::StorageFault;
  Record candidate=record_;candidate.flags|=Maintenance;return commit(candidate);
}
bool Ledger::restoreController(watering::AutonomousController& c,uint32_t now) const {
  watering::AutonomousRestore r;
  r.initialized=loaded_&&!fault_;r.episode=record_.flags&Episode;
  r.pending=record_.flags&Pending;r.maintenance=record_.flags&Maintenance;
  r.cooldown=record_.flags&Cooldown;r.waiting=record_.flags&Waiting;
  r.cycle=record_.cycle;r.total=record_.total;return c.restore(r,now);
}
}  // namespace autonomous_storage
