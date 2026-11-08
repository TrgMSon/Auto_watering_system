#include "pump/Arduino.h"
#include "../esp32/ble_auth.h"
#include "../esp32/ble_command_bridge.h"
#include "../esp32/ble_telemetry.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
using namespace ble_commands;
uint32_t fakeNow=0;
int outputLevel=-1,writes=0,outputModes=0;
namespace {
const char fixture[]="test-ble-secret-key";
struct Keys : KeySource {
 bool load(uint8_t* out,size_t capacity,size_t& size) const override {
  size=sizeof(fixture)-1; if(capacity<size) return false; memcpy(out,fixture,size); return true;
 }
};
struct CryptoImpl : Crypto {
 bool fail=false;
 bool random(uint8_t (&out)[8]) override {
  const uint8_t bytes[]={0x84,0xa9,0xf2,0x1b,0x3c,0x0e,0x5a,0x1d}; memcpy(out,bytes,8); return !fail;
 }
 bool hmac(const uint8_t* key,size_t n,const char* msg,size_t len,uint8_t (&out)[32]) override {
  unsigned size=0; return !fail && HMAC(EVP_sha256(),key,int(n),reinterpret_cast<const uint8_t*>(msg),len,out,&size) && size==32;
 }
};
struct Wall : Clock {
 uint32_t utc=0; unsigned calls=0; bool fail=false;
 bool setUtc(uint32_t seconds) override { ++calls; if(fail) return false; utc=seconds; return true; }
};
std::string nonce(Authenticator& a) { const char* p; uint16_t n; assert(a.readNonce(0,fakeNow,p,n)); return std::string(p,n); }
std::string sign(CryptoImpl& c,const std::string& canonical) {
 uint8_t d[32]; assert(c.hmac(reinterpret_cast<const uint8_t*>(fixture),sizeof(fixture)-1,canonical.data(),canonical.size(),d));
 std::string s; const char h[]="0123456789abcdef";
 for(unsigned i=0;i<8;++i) { s+=h[d[i]>>4]; s+=h[d[i]&15]; } return s;
}
std::string timePayload(const std::string& value,const std::string& sig="0000000000000000") {
 return "{\"unix\":"+value+",\"sig\":\""+sig+"\"}";
}
Authentication sendTime(Authenticator& a,const std::string& p) { return a.authenticate(p.data(),p.size(),fakeNow,Op::Time); }
Command timeCommand(uint32_t seconds=kUnixMin) { Command c; c.op=Op::Time; c.unixSeconds=seconds; return c; }
sensors::Reading sample() { sensors::Reading r; r.sampledAtMs=fakeNow; r.soilValid=r.distanceValid=true; r.soilPercent=20; r.distanceCm=5; return r; }
}
int main() {
 assert(authenticatedWrite(2) && authenticatedWrite(7));
 assert(authenticatedWrite(3) && !authenticatedWrite(6)); // Same policy used by native GATT table.
 CryptoImpl crypto; Keys keys; Authenticator auth(crypto); Wall wall; auth.loadKey(&keys);
 assert(sign(crypto,"84a9f21b3c0e5a1d|TIME|1793800000")=="26b87998f4bc4778");
 nonce(auth); auto valid=timePayload("1793800000","26b87998f4bc4778");
 auto authenticated=sendTime(auth,valid); assert(authenticated.code==Code::Ok && authenticated.command.op==Op::Time);
 assert(wall.calls==0);
 nonce(auth); assert(sendTime(auth,timePayload("1793800000")).code==Code::BadSig);
 nonce(auth); crypto.fail=true; assert(sendTime(auth,valid).missingKey); crypto.fail=false;
 Authenticator missing(crypto); nonce(missing); assert(sendTime(missing,valid).missingKey);
 Command parsed;
 for(const char* value : {"true","null","\"1793800000\"","1793800000.0","1e9","01793800000","[]","{}","+1793800000"}) {
  auto p=timePayload(value); assert(parse(p.data(),p.size(),parsed,Op::Time)==Code::BadJson);
 }
 for(const char* value : {"-1","0","1704067199","4102444800","4294967295","4294967296","18446744073709551616"}) {
  auto p=timePayload(value); assert(parse(p.data(),p.size(),parsed,Op::Time)==Code::InvalidTime);
 }
 for(uint32_t value : {kUnixMin,kUnixMax}) {
  auto p=timePayload(std::to_string(value),sign(crypto,nonce(auth)+"|TIME|"+std::to_string(value)));
  auto a=sendTime(auth,p); assert(a.code==Code::Ok && a.command.unixSeconds==value);
 }
 for(const std::string& p : {std::string("{}"),std::string("{\"unix\":1793800000}"),
     std::string("{\"unix\":1793800000,\"unix\":1793800000}"),
     std::string("{\"sig\":\"0000000000000000\",\"sig\":\"0000000000000000\"}"),
     valid+"x", valid.substr(0,valid.size()-1), std::string(129,'x'),
     std::string("{\"unix\":1793800000,\"sig\":\"0000000000000000\",\"extra\":0}")})
  assert(parse(p.data(),p.size(),parsed,Op::Time)==Code::BadJson);
 std::string reordered=" {\"sig\":\"26b87998f4bc4778\",\"unix\":1793800000} ";
 nonce(auth); assert(sendTime(auth,reordered).code==Code::Ok);
 assert(parse(valid.data(),valid.size(),parsed)==Code::BadJson); // TIME cannot enter pump endpoint.
 const std::string off="{\"cmd\":\"PUMP_OFF\",\"dur\":0,\"sig\":\"238041d3721ef169\"}";
 assert(parse(off.data(),off.size(),parsed,Op::Time)==Code::BadJson);
 nonce(auth); assert(sendTime(auth,valid).code==Code::Ok);
 assert(auth.authenticate(off.data(),off.size(),fakeNow).code==Code::NonceExpired);
 nonce(auth); assert(auth.authenticate(off.data(),off.size(),fakeNow).code==Code::Ok);
 assert(sendTime(auth,valid).code==Code::NonceExpired);
 nonce(auth); assert(sendTime(auth,"bad").code==Code::BadJson);
 assert(auth.authenticate(off.data(),off.size(),fakeNow).code==Code::NonceExpired);
 // Wall jumps do not affect TTL, including monotonic rollover.
 for(uint32_t start : {0u,UINT32_MAX-100u}) {
  fakeNow=start; nonce(auth); wall.setUtc(kUnixMax); fakeNow+=29999;
  assert(sendTime(auth,valid).code==Code::Ok);
  fakeNow=start; nonce(auth); wall.setUtc(kUnixMin); fakeNow+=30000;
  assert(sendTime(auth,valid).code==Code::NonceExpired);
 }
 fakeNow=100; irrigation::System system; system.begin(); system.update(sample());
 wall=Wall{};
 auto g=ble_command_bridge::connect(7); assert(g);
 assert(ble_command_bridge::reserve(g,1,authenticated.command,Code::Ok,fakeNow));
 Dispatch::Slot result;
 assert(!ble_command_bridge::result(result) && wall.calls==0);
 ble_command_bridge::serviceOwner(system,&wall);
 assert(wall.calls==1 && wall.utc==1793800000);
 assert(ble_command_bridge::result(result) && result.code==Code::Ok);
 char json[kResultCapacity]; resultJson(result.command.op,result.code,json);
 assert(!strcmp(json,"{\"op\":\"TIME\",\"code\":\"OK\"}")); ble_command_bridge::release(result.ticket);
 wall.fail=true;
 assert(ble_command_bridge::reserve(g,2,timeCommand(),Code::Ok,fakeNow));
 ble_command_bridge::serviceOwner(system,&wall);
 assert(ble_command_bridge::result(result) && result.code==Code::None); ble_command_bridge::release(result.ticket);
 wall.fail=false;
 unsigned before=wall.calls;
 assert(ble_command_bridge::reserve(g,3,timeCommand(),Code::BadSig,fakeNow));
 ble_command_bridge::serviceOwner(system,&wall);
 assert(wall.calls==before && ble_command_bridge::result(result) && result.code==Code::BadSig);
 ble_command_bridge::release(result.ticket);
 assert(ble_command_bridge::reserve(g,4,timeCommand(),Code::Ok,fakeNow));
 ble_command_bridge::disconnect(7); auto newg=ble_command_bridge::connect(7); assert(newg!=g);
 ble_command_bridge::serviceOwner(system,&wall); assert(wall.calls==before && !ble_command_bridge::result(result));
 g=newg;
 // A completed old result cannot cross a disconnect either.
 assert(ble_command_bridge::reserve(g,5,timeCommand(),Code::Ok,fakeNow)); ble_command_bridge::serviceOwner(system,&wall);
 ble_command_bridge::disconnect(7); g=ble_command_bridge::connect(7); assert(!ble_command_bridge::result(result));
 before=wall.calls;
 assert(ble_command_bridge::reserve(g,6,timeCommand(),Code::Ok,fakeNow));
 wall.utc=kUnixMax; fakeNow+=1000; ble_command_bridge::serviceOwner(system,&wall);
 assert(wall.calls==before && ble_command_bridge::result(result) && result.code==Code::None); ble_command_bridge::release(result.ticket);
 // Saturating general slots with TIME leaves OFF available and admitted first.
 Dispatch d; auto dg=d.connect(1);
 for(unsigned i=0;i<3;++i) assert(d.reserve(dg,i,timeCommand(),Code::Ok,fakeNow));
 assert(!d.reserve(dg,4,timeCommand(),Code::Ok,fakeNow));
 Command stop; stop.op=Op::Off; assert(d.reserve(dg,5,stop,Code::Ok,fakeNow));
 assert(d.claim(result) && result.command.op==Op::Off); d.complete(result.ticket,Code::Ok);
 assert(d.claim(result) && result.command.op==Op::Time); // OFF does not cancel unrelated TIME.
 d.disconnect(1); d.connect(1); d.complete(result.ticket,Code::Ok); assert(!d.result(result));
 // TIME has no irrigation side effects, even during an active run.
 fakeNow=2000; system.begin(); system.update(sample()); pump::testGuardEnable(true);
 assert(system.startManual(5)==irrigation::Result::Accepted);
 const auto run=pump::run(); const auto status=system.status(); const int gpioWrites=writes;
 const auto config=system.configurationStatus().active();
 for(uint32_t utc : {kUnixMax,kUnixMin}) {
  assert(execute(system,timeCommand(utc),&wall)==Code::Ok);
  assert(writes==gpioWrites && pump::isOn());
  assert(pump::run().startedMs==run.startedMs && pump::run().durationMs==run.durationMs && pump::run().generation==run.generation);
  assert(system.status().mode==status.mode && system.status().remainingMs==status.remainingMs);
  assert(system.status().decision.state==status.decision.state && system.status().decision.reason==status.decision.reason);
  assert(system.status().decision.cooldownRemainingMs==status.decision.cooldownRemainingMs);
  assert(system.configurationStatus().active().fullCm==config.fullCm && system.configurationStatus().active().emptyCm==config.emptyCm);
 }
 fakeNow+=250; pump::testGuardInterrupt(); assert(!pump::isOn()); // TIME did not renew safety lease.
 assert(pump::takeStopEvent().cause==pump::StopCause::SafetyLeaseExpired);
 system.service();
 const auto stopped=system.status();
 assert(execute(system,timeCommand(kUnixMax),&wall)==Code::Ok);
 assert(system.status().decision.cooldownRemainingMs==stopped.decision.cooldownRemainingMs);
 assert(system.status().decision.wateringAttempts==stopped.decision.wateringAttempts);
 assert(execute(system,timeCommand(0),&wall)==Code::InvalidTime);
 assert(execute(system,timeCommand(),nullptr)==Code::None);
 // Sensor and publication freshness still use fakeNow, never wall.utc.
 ble_telemetry::Snapshot snap; snap.reading=sample(); snap.publishedAtMs=fakeNow; snap.actuatorOn=false;
 wall.utc=kUnixMax; assert(ble_telemetry::serve(snap,fakeNow).size);
 wall.utc=kUnixMin; assert(!ble_telemetry::serve(snap,fakeNow+251).size);
 // Re-publication cannot refresh old sensor readings, even with wall jumps.
 snap.publishedAtMs=fakeNow+4001; wall.utc=kUnixMax;
 auto stale=ble_telemetry::serve(snap,fakeNow+4001);
 assert(stale.size && std::string(stale.bytes,stale.size).find("\"soil\":null")!=std::string::npos);
 ble_telemetry::Session session; session.connect(3,fakeNow); session.setMtu(3,185); session.subscribe(3,1,false);
 wall.utc=kUnixMin; assert(session.notification(fakeNow+1999,50)==ble_telemetry::Session::Notify::None);
 wall.utc=kUnixMax; assert(session.notification(fakeNow+2000,50)==ble_telemetry::Session::Notify::Send);
 // Pending timeout is exactly 1000 monotonic ms for either wall jump, including wrap.
 for(uint32_t start : {100u,UINT32_MAX-100u}) {
  for(uint32_t utc : {kUnixMin,kUnixMax}) {
   Dispatch pending; auto pg=pending.connect(4); assert(pending.reserve(pg,1,timeCommand(),Code::Ok,start));
   wall.utc=utc; pending.expire(start+999); assert(!pending.result(result));
   pending.expire(start+1000); assert(pending.result(result) && result.code==Code::None);
   assert(!pending.claim(result));
  }
 }
 std::cout<<"PASS: TIME vector/parser/shared nonce, clock failures, generation/OFF priority, independent monotonic guard/freshness\n";
}

