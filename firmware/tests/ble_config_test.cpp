#include "pump/Arduino.h"
#include "../esp32/ble_auth.h"
#include "../esp32/ble_command_bridge.h"
#include "../esp32/mqtt_commands.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
using namespace ble_commands;
uint32_t fakeNow=0;
int outputLevel=-1,writes=0,outputModes=0;
namespace {
const char fixture[]="test-ble-secret-key";
const char knownNonce[]="84a9f21b3c0e5a1d";
const char vectorSig[]="a4426d27c5dddb70";
struct Keys : KeySource {
 std::string bytes=fixture;
 bool load(uint8_t* p,size_t cap,size_t& n) const override { n=bytes.size(); if(n>cap) return false; memcpy(p,bytes.data(),n); return true; }
};
struct HostCrypto : Crypto {
 bool fail=false;
 bool random(uint8_t (&out)[8]) override { const uint8_t b[]={0x84,0xa9,0xf2,0x1b,0x3c,0x0e,0x5a,0x1d}; memcpy(out,b,8); return !fail; }
 bool hmac(const uint8_t* key,size_t n,const char* msg,size_t len,uint8_t (&out)[32]) override {
  unsigned size=0; return !fail && HMAC(EVP_sha256(),key,int(n),reinterpret_cast<const uint8_t*>(msg),len,out,&size) && size==32;
 }
};
std::string sign(const std::string& key,const std::string& message) {
 uint8_t digest[32]; unsigned n=0;
 assert(HMAC(EVP_sha256(),key.data(),int(key.size()),reinterpret_cast<const uint8_t*>(message.data()),message.size(),digest,&n) && n==32);
 const char hex[]="0123456789abcdef"; std::string s;
 for(unsigned i=0;i<8;++i) {s+=hex[digest[i]>>4];s+=hex[digest[i]&15];} return s;
}
std::vector<std::string> fields={"\"min\":30.0","\"max\":70.0","\"auto\":1","\"max_dur\":120","\"min_water\":20.0",std::string("\"sig\":\"")+vectorSig+"\""};
std::string json(const std::vector<std::string>& f) { std::string s="{"; for(const auto& x:f) { if(s.size()>1)s+=",";s+=x; } return s+"}"; }
std::string changed(unsigned i,const std::string& value) { auto f=fields; f[i]=f[i].substr(0,f[i].find(':')+1)+value; return json(f); }
Code parseString(const std::string& p,ConfigRequest& r) { return parseConfig(p.data(),p.size(),r); }
void nonce(Authenticator& a,uint32_t now=0) { const char* p; uint16_t n; assert(a.readNonce(0,now,p,n)&&n==16&&!memcmp(p,knownNonce,n)); }
ConfigAuthentication send(Authenticator& a,const std::string& p,uint32_t now=0) {return a.authenticateConfig(p.data(),p.size(),now);}
}
// Real System + dispatch/bridge, fault-injected flash. No GATT/radio mock success.
struct Flash : configuration::Storage, desired_mode::Storage {
 std::vector<uint8_t> blob; uint8_t mode=1;
 std::vector<std::string> trace;
 unsigned operations=0,failAt=0; bool failAfter=false;
 irrigation::System* owner=nullptr;
 bool boundary(const char* label) {trace.push_back(label);++operations;return operations!=failAt||failAfter;}
 bool finish() {return operations!=failAt;}
 void off() {assert(owner && owner->status().mode==irrigation::Mode::Manual && !pump::isOn());}
 bool readMode(uint8_t& v) override {if(!boundary("read-mode"))return false;v=mode;return finish();}
 bool writeMode(uint8_t v) override {off();if(!boundary(v?"write-AUTO":"write-MANUAL"))return false;mode=v;return finish();}
 bool read(uint8_t* v,size_t cap,size_t& n) override {
  n=0;if(!boundary("read-config")||blob.size()>cap)return false;n=blob.size();memcpy(v,blob.data(),n);return finish();
 }
 bool write(const uint8_t* v,size_t n) override {off();if(!boundary("write-config"))return false;blob.assign(v,v+n);return finish();}
 void seed() {configuration::Config c;c.fullCm=2.5f;c.emptyCm=12;blob.resize(configuration::kRecordSize);configuration::encode(c,blob.data());}
 void clear() {trace.clear();operations=failAt=0;failAfter=false;}
};
sensors::Reading sample(float soil=80) {sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.distanceValid=true;r.soilPercent=soil;r.distanceCm=5;return r;}
void ownerTests() {
 Keys keys;HostCrypto crypto;Authenticator auth(crypto);auth.loadKey(&keys);
 // Use a signed decimal request, preserving tenths all the way into Config.waterMin.
 auto f=fields;f[0]="\"min\":35.0";f[4]="\"min_water\":20.1";
 const std::string canon="84a9f21b3c0e5a1d|CONFIG|35.0|70.0|1|120|20.1";
 f[5]="\"sig\":\""+sign(fixture,canon)+"\"";
 nonce(auth);const auto a=send(auth,json(f));assert(a.code==Code::Ok);
 Command command;command.op=Op::Config;command.config=a.request;
 const std::vector<std::string> order={"write-MANUAL","read-mode","write-config","read-config","write-AUTO","read-mode"};
 for(unsigned boundary=0;boundary<=6;++boundary) for(bool after:{false,true}) {
  Flash flash;flash.seed();irrigation::System system;flash.owner=&system;system.begin(&flash,&flash);system.update(sample());flash.clear();
  const auto previous=system.configurationStatus().active();flash.failAt=boundary;flash.failAfter=after;
  const auto generation=ble_command_bridge::connect(31);assert(generation);
  assert(ble_command_bridge::reserve(generation,42,command,a.code,fakeNow));
  Dispatch::Slot result;assert(!ble_command_bridge::result(result)&&flash.trace.empty()); // auth is not OK/applied
  ble_command_bridge::serviceOwner(system);
  assert(ble_command_bridge::result(result)&&result.command.op==Op::Config);
  assert(result.code==(boundary?Code::StorageError:Code::Ok));
  char out[kResultCapacity];assert(resultJson(result.command.op,result.code,out));
  assert(!strcmp(out,boundary?"{\"op\":\"CONFIG\",\"code\":\"STORAGE_ERROR\"}":"{\"op\":\"CONFIG\",\"code\":\"OK\"}"));
  auto active=system.configurationStatus().active();assert(active.fullCm==2.5f&&active.emptyCm==12);
  if(boundary) {
   assert(system.status().mode==irrigation::Mode::Manual&&!system.status().pumpOn);
   assert(active.soilStart==(boundary<=4?previous.soilStart:35));
   assert(flash.operations==boundary);
  } else {
   assert(flash.trace==order&&active.waterMin==20.1f&&active.soilStart==35&&system.status().mode==irrigation::Mode::Auto);
   flash.clear();assert(execute(system,command)==Code::Ok&&flash.trace.empty()); // no-op
   auto bad=command;bad.config.minWaterPercentTenths=50;
   assert(execute(system,bad)==Code::OutOfRange&&flash.trace.empty()&&system.status().mode==irrigation::Mode::Auto);
   system.update(sample(20));assert(system.status().pumpOn);assert(execute(system,command)==Code::PumpBusy&&flash.trace.empty());system.stopManual();
  }
  ble_command_bridge::release(result.ticket);ble_command_bridge::disconnect(31);
 }
 // A legal wire threshold can still violate the full calibrated candidate.
 {
  Flash flash;configuration::Config c;c.fullCm=3.5f;c.waterMin=30;
  flash.blob.resize(configuration::kRecordSize);configuration::encode(c,flash.blob.data());
  irrigation::System system;flash.owner=&system;system.begin(&flash,&flash);system.update(sample());flash.clear();
  auto invalid=command;invalid.config.minWaterPercentTenths=200;
  assert(execute(system,invalid)==Code::OutOfRange&&flash.trace.empty());
  assert(configuration::equal(c,system.configurationStatus().active())&&system.status().mode==irrigation::Mode::Auto);
 }
 // Pending CONFIG shares ON's lifecycle: OFF/expiry returns no business code;
 // disconnect discards the result. No stale AUTO, no invented cancellation code.
 for(unsigned cancel=0;cancel<4;++cancel) {
  Flash flash;flash.seed();irrigation::System system;flash.owner=&system;system.begin(&flash,&flash);system.update(sample());flash.clear();
  const auto g=ble_command_bridge::connect(32);const auto epoch=plant_mqtt::commandIngressEpoch();
  assert(ble_command_bridge::reserve(g,1,command,Code::Ok,fakeNow,epoch));
  if(cancel==0) {Command off;off.op=Op::Off;assert(ble_command_bridge::reserve(g,2,off,Code::Ok,fakeNow));}
  if(cancel==1) ble_command_bridge::disconnect(32);
  if(cancel==2) {fakeNow+=1000;ble_command_bridge::expire(fakeNow);}
  if(cancel==3) {plant_mqtt::commandConnection(true);plant_mqtt::PumpCommand off;strcpy(off.id,"config-cancel-off");assert(plant_mqtt::queueCommand(off,fakeNow));}
  ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);
  assert(system.configurationStatus().active().soilStart==30&&!system.status().pumpOn);
  for(const auto& label:flash.trace) assert(label!="write-config"&&label!="write-AUTO");
  if(cancel==0||cancel==3) assert(!ble_command_bridge::reserve(g,3,command,Code::Ok,fakeNow,epoch));
  Dispatch::Slot r;while(ble_command_bridge::result(r)){if(r.command.op==Op::Config)assert(r.code==Code::None);ble_command_bridge::release(r.ticket);}
  ble_command_bridge::disconnect(32);plant_mqtt::commandConnection(false);
 }
 // Admitted CONFIG is allowed to finish after disconnect; obsolete result is discarded.
 Dispatch d;auto g=d.connect(1);assert(d.reserve(g,1,command,Code::Ok,0));Dispatch::Slot r;
 assert(d.claim(r));d.disconnect(1);d.complete(r.ticket,Code::Ok);assert(!d.result(r));
 assert(configWriteFits(182,185)&&!configWriteFits(183,185));
 assert(configWriteFits(192,195)&&!configWriteFits(193,517)&&!configWriteFits(1,2));
 assert(!configWriteFits(90,92)&&configWriteFits(90,93));
 assert(!configWriteFits(90,185,true)&&!configWriteFits(90,185,false,false)&&!configWriteFits(90,185,false,true,1));
}

int main() {
 const std::string canonical="84a9f21b3c0e5a1d|CONFIG|30.0|70.0|1|120|20.0";
 assert(sign(fixture,canonical)==vectorSig); // Independent OpenSSL, not firmware canonical helper.
 Keys keys;HostCrypto crypto;Authenticator auth(crypto);auth.loadKey(&keys);
 ConfigRequest r;const auto payload=json(fields);
 assert(parseString(payload,r)==Code::Ok && r.minPercentTenths==300 && r.maxPercentTenths==700 && r.minWaterPercentTenths==200);
 char buffer[kConfigCanonicalCapacity];assert(configCanonical(knownNonce,r,buffer,sizeof(buffer))==canonical.size()&&buffer==canonical);
 nonce(auth);assert(send(auth,payload).code==Code::Ok);
 nonce(auth);assert(send(auth,changed(5,"\"0000000000000000\"")).code==Code::BadSig);
 // Raw UTF-8 bytes, whitespace and Base64-looking characters must be used literally.
 keys.bytes=" \xc3\xa9+/= ";auth.loadKey(&keys);
 nonce(auth);assert(send(auth,changed(5,"\""+sign(keys.bytes,canonical)+"\"")).code==Code::Ok);
 nonce(auth);assert(send(auth,payload).code==Code::BadSig);
 auth.loadKey(&keys);nonce(auth);crypto.fail=true;assert(send(auth,payload).missingKey);crypto.fail=false;
 auth.loadKey(nullptr);nonce(auth);assert(send(auth,payload).missingKey);keys.bytes=fixture;auth.loadKey(&keys);
 // All permutations must produce the same canonical, not JSON field order.
 std::vector<unsigned> order={0,1,2,3,4,5};
 do { std::vector<std::string> f; for(auto i:order)f.push_back(fields[i]);
  assert(parseString(json(f),r)==Code::Ok); assert(configCanonical(knownNonce,r,buffer,sizeof(buffer))==canonical.size()&&buffer==canonical);
 } while(std::next_permutation(order.begin(),order.end()));
 for(unsigned i=0;i<6;++i) {
  auto f=fields;f.erase(f.begin()+i);assert(parseString(json(f),r)==Code::BadJson);
  f=fields;f.insert(f.begin(),fields[i]);assert(parseString(json(f),r)==Code::BadJson);
  for(const char* v:{"null","true","[]","{}","NaN","Infinity"}) assert(parseString(changed(i,v),r)==Code::BadJson);
 }
 for(unsigned i=0;i<5;++i) assert(parseString(changed(i,"\"30\""),r)==Code::BadJson);
 assert(parseString(changed(5,"\"30\""),r)==Code::BadSig);
 auto unknown=fields;unknown.push_back("\"other\":1");assert(parseString(json(unknown),r)==Code::BadJson);
 assert(parseString(payload+"x",r)==Code::BadJson);assert(parseString(payload.substr(0,payload.size()-1),r)==Code::BadJson);
 auto nul=payload;nul.insert(1,1,'\0');assert(parseString(nul,r)==Code::BadJson);
 for(unsigned i:{0u,1u,4u}) {
  for(const char* value:{"1.00","1.01","1e1","1E+1","01",".1","1.","+1","1 .0"})
   assert(parseString(changed(i,value),r)==Code::BadJson);
  for(const char* value:{"-1","-0","429496729.6","429496730","18446744073709551616"})
   assert(parseString(changed(i,value),r)==Code::OutOfRange);
 }
 for(unsigned i:{2u,3u}) {
  for(const char* value:{"1.0","1e2","01","+1"}) assert(parseString(changed(i,value),r)==Code::BadJson);
  assert(parseString(changed(i,"4294967296"),r)==Code::OutOfRange);
 }
 for(const char* v:{"0","0.0","0.1","69.9"}) assert(parseString(changed(0,v),r)==Code::Ok);
 for(const char* v:{"30","30.0","100.1"}) assert(parseString(changed(1,v),r)==Code::OutOfRange);
 assert(parseString(changed(1,"100.0"),r)==Code::Ok);
 for(const char* v:{"0","1"}) assert(parseString(changed(2,v),r)==Code::Ok);
 for(const char* v:{"-1","2"}) assert(parseString(changed(2,v),r)==Code::OutOfRange);
 for(const char* v:{"60","600"}) assert(parseString(changed(3,v),r)==Code::Ok);
 for(const char* v:{"59","601"}) assert(parseString(changed(3,v),r)==Code::OutOfRange);
 for(const char* v:{"20","20.0","20.1","99.9","100.0"}) assert(parseString(changed(4,v),r)==Code::Ok);
 for(const char* v:{"5.0","19.9","100.1"}) assert(parseString(changed(4,v),r)==Code::OutOfRange);
 // Integer JSON values canonicalize exactly to .0, no float conversion/rounding.
 assert(parseString(changed(0,"30"),r)==Code::Ok);configCanonical(knownNonce,r,buffer,sizeof(buffer));assert(buffer==canonical);
 auto maximum=fields;maximum[0]="\"min\":99.9";maximum[1]="\"max\":100.0";maximum[4]="\"min_water\":100.0";
 const auto compactMax=json(maximum);assert(compactMax.size()==90 && parseString(compactMax,r)==Code::Ok);
 assert(configCanonical(knownNonce,r,buffer,sizeof(buffer))>0);
 // Canonical output capacity boundaries and canaries.
 assert(parseString(payload,r)==Code::Ok);
 struct Guarded {char before='X';char out[64]={};char after='Y';} guarded;
 assert(configCanonical(knownNonce,r,guarded.out,canonical.size())==0 && guarded.out[0]==0);
 assert(configCanonical(knownNonce,r,guarded.out,canonical.size()+1)==canonical.size());
 assert(guarded.before=='X'&&guarded.after=='Y');assert(!configCanonical("bad",r,buffer,sizeof(buffer)));
 std::string padded=payload+std::string(kConfigPayloadMax-payload.size(),' ');
 assert(parseString(padded,r)==Code::Ok);assert(parseString(padded+" ",r)==Code::BadJson);
 assert(parseConfig(nullptr,1,r)==Code::BadJson && parseConfig(payload.data(),0,r)==Code::BadJson);
 // Shared nonce cross-replay in every direction; no CONFIG dispatch or GATT needed.
 const std::string pump="{\"cmd\":\"PUMP_OFF\",\"dur\":0,\"sig\":\"238041d3721ef169\"}";
 const std::string time="{\"unix\":1793800000,\"sig\":\"26b87998f4bc4778\"}";
 for(unsigned first=0;first<3;++first) for(unsigned second=0;second<3;++second) {
  nonce(auth);auto call=[&](unsigned which) {return which==0 ? send(auth,payload).code : which==1 ? auth.authenticate(time.data(),time.size(),0,Op::Time).code : auth.authenticate(pump.data(),pump.size(),0).code;};
  assert(call(first)==Code::Ok);assert(call(second)==Code::NonceExpired);
 }
 nonce(auth);assert(send(auth,"{}").code==Code::BadJson);assert(auth.authenticate(pump.data(),pump.size(),0).code==Code::NonceExpired);
 // Rejected CONFIG must burn the shared nonce too, including parse-range and
 // signature failures. A later valid CONFIG/TIME/pump cannot reuse that nonce.
 for(const auto& rejected: {changed(2,"2"), changed(3,"59"),
     changed(5,"\"0000000000000000\""), changed(5,"\"ABCDEF0123456789\"")}) {
  nonce(auth);assert(send(auth,rejected).code!=Code::Ok);
  assert(send(auth,payload).code==Code::NonceExpired);
  assert(auth.authenticate(time.data(),time.size(),0,Op::Time).code==Code::NonceExpired);
  assert(auth.authenticate(pump.data(),pump.size(),0).code==Code::NonceExpired);
 }
 // A fresh nonce explicitly issued after rejection permits the valid request.
 nonce(auth);assert(send(auth,payload).code==Code::Ok);
 nonce(auth);assert(send(auth,padded+" ").code==Code::BadJson);assert(send(auth,payload).code==Code::NonceExpired);
 for(uint32_t start:{0u,UINT32_MAX-10}) {nonce(auth,start);assert(send(auth,payload,start+29999).code==Code::Ok);nonce(auth,start);assert(send(auth,payload,start+30000).code==Code::NonceExpired);}
 nonce(auth);auth.reset();assert(send(auth,payload).code==Code::NonceExpired);
 assert(authenticatedWrite(2)&&authenticatedWrite(7)&&authenticatedWrite(3)&&!authenticatedWrite(6));
 // Pump parser remains separate from CONFIG.
 Command command;assert(parse(payload.data(),payload.size(),command)==Code::BadJson);
 ownerTests();
 std::cout<<"PASS: CONFIG independent HMAC/vector/raw UTF-8, exact tenths/schema/ranges/bounds/permutations/shared nonce; real owner/persistence faults/no-op/cancellation/MTU/results\n";
}
