#include "../esp32/ble_wifi_handoff.h"
#include "../esp32/ble_pairing.h"
#include "../esp32/ble_dispatch.h"
#include "pump/Arduino.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <string>
#include <iostream>
using namespace ble_commands;
uint32_t fakeNow=0;int outputLevel=-1,writes=0,outputModes=0;
struct Keys:KeySource{bool load(uint8_t* p,size_t cap,size_t& n)const override{const char k[]="test-ble-secret-key";n=sizeof(k)-1;if(cap<n)return false;memcpy(p,k,n);return true;}};
struct CryptoImpl:Crypto{
 bool random(uint8_t (&p)[8])override{const uint8_t n[]={0x84,0xa9,0xf2,0x1b,0x3c,0x0e,0x5a,0x1d};memcpy(p,n,8);return true;}
 bool hmac(const uint8_t* k,size_t n,const char* m,size_t l,uint8_t (&out)[32])override{unsigned size=0;return HMAC(EVP_sha256(),k,int(n),reinterpret_cast<const uint8_t*>(m),l,out,&size)&&size==32;}
};
void nonce(Authenticator& a){const char* p;uint16_t n;assert(a.readNonce(0,0,p,n));}
// Source-faithful boundary shim: gatt_db permission gate precedes APP_RSP handling.
// It models IDF ordering, not a replacement BLE stack or hardware proof.
struct StackShim{
 unsigned callbacks=0,copies=0;
 template<class Callback> unsigned write(bool encrypted,bool keyPresent,bool appResponse,Callback callback){
  (void)appResponse;
  if(!keyPresent)return 0x05;
  if(!encrypted)return 0x0f;
  ++callbacks;callback();return 0;
 }
};
struct Radio:ble_wifi::Radio{bool ip=false;unsigned starts=0;bool startRam(const WifiRequest&)override{++starts;return true;}State poll()override{return ip?State::CandidateIp:State::Pending;}void stopCandidate()override{}};
struct Store:ble_wifi::Storage{ble_wifi::Persistence outcome=ble_wifi::Persistence::Verified;unsigned calls=0;ble_wifi::Persistence commitVerified(const WifiRequest&)override{++calls;return outcome;}};
int main(){
 assert(encryptedWifiWrite(6)&&!encryptedWifiWrite(3)&&!authenticatedWrite(6)&&authenticatedWrite(2)&&authenticatedWrite(7));
 uint8_t peer[]={1,2,3,4,5,6},wrong[]={6,5,4,3,2,1};ble_pairing::Binding binding;binding.connect(peer,7);binding.bonded(7,true);
 ble_wifi::WriteBuffer raw;
 char local[ble_wifi::kWriteCapacity]={};size_t copiedSize=0;
 assert(!raw.take(1,7,local,sizeof(local),copiedSize));
 uint32_t ticket=raw.store(7,"fixture",7);assert(ticket&&!raw.store(7,"second",6));
 assert(!raw.take(ticket,8,local,sizeof(local),copiedSize));
 assert(raw.take(ticket,7,local,sizeof(local),copiedSize)&&copiedSize==7);
 assert(!raw.take(ticket,7,local,sizeof(local),copiedSize));
 assert(!raw.store(7,local,ble_wifi::kWriteCapacity+1));
 ticket=raw.store(7,"fixture",7);raw.clear();assert(!raw.take(ticket,7,local,sizeof(local),copiedSize));
 StackShim stack;
 assert(stack.write(false,true,true,[&](){++stack.copies;})==0x0f&&stack.callbacks==0&&stack.copies==0);
 assert(stack.write(false,true,false,[&](){++stack.copies;})==0x0f&&stack.callbacks==0);
 assert(!binding.captureWrite(wrong,7)&&!binding.captureWrite(peer,6));
 uint64_t evidence=0;
 assert(!stack.write(true,true,true,[&](){evidence=binding.captureWrite(peer,7);if(evidence)++stack.copies;}));assert(evidence==7&&stack.copies==1);
 WifiSecurity security;security.connect(7);security.update(7,true,true,false);
 assert(!security.permits(7)&&security.permitsWrite(7,evidence)); // Never claim connection encrypted.
 CryptoImpl crypto;Keys keys;Authenticator auth(crypto);auth.loadKey(&keys);
 const std::string p="{\"ssid\":\"HomeWiFi\",\"pass\":\"12345678\",\"sig\":\"ca81265e275b9a4a\"}";
 nonce(auth);auto denied=auth.authenticateWifi(reinterpret_cast<const char*>(uintptr_t(1)),p.size(),0,security,7,185);
 assert(denied.securityDenied);
 nonce(auth);auto accepted=auth.authenticateWifi(p.data(),p.size(),0,security,7,185,false,true,0,evidence);assert(accepted.code==Code::Ok);
 assert(accepted.encryptedWriteGeneration==7);
 ble_wifi::Handoff slot;slot.connection(7);
 assert(slot.reserve(accepted,0)); // This is the immediate ATT ACK point, before radio connect/commit.
 Radio radio;Store store;assert(!radio.starts&&!store.calls);
 ble_wifi::Attempt attempt;WifiAuthentication work;
 assert(slot.take(0,work));assert(attempt.reserve(7,work,true,false,185,0));
 attempt.service(0,7,radio,store,security);assert(radio.starts==1&&!store.calls);
 attempt.service(3000,7,radio,store,security);assert(attempt.state()==ble_wifi::Attempt::State::Connecting&&!store.calls);
 radio.ip=true;attempt.service(5000,7,radio,store,security);Code result;
 assert(attempt.result(7,5000,result)&&result==Code::WifiOk);slot.complete(7,result,5000);
 uint64_t generation;assert(slot.result(5000,generation,result)&&generation==7&&result==Code::WifiOk);
 char wire[64];assert(ble_wifi::wifiResult(result,wire,sizeof(wire))&&std::string(wire)=="{\"op\":\"WIFI\",\"code\":\"WIFI_OK\"}");
 slot.release(7);attempt.release();
 assert(slot.reserve(accepted,6000));slot.connection(8);assert(!slot.take(6000,work));
 slot.connection(7);assert(slot.reserve(accepted,6000));assert(slot.take(6000,work));slot.connection(8);slot.complete(7,Code::WifiOk,7000);assert(!slot.result(7000,generation,result));
 for(auto outcome:{ble_wifi::Persistence::NotCommitted,ble_wifi::Persistence::Unconfirmed}){
  ble_wifi::Attempt failed;Radio r;Store s;s.outcome=outcome;
  assert(failed.reserve(7,accepted,true,false,185,0));failed.service(0,7,r,s,security);r.ip=true;failed.service(10,7,r,s,security);
  assert(failed.result(7,10,result)&&result==Code::WifiFail&&s.calls==1);
 }
 for(uint32_t start:{0u,UINT32_MAX-100u}){ble_wifi::Attempt timeout;Radio r;Store s;
  assert(timeout.reserve(7,accepted,true,false,185,start));timeout.service(start,7,r,s,security);
  timeout.service(start+30000,7,r,s,security);assert(timeout.result(7,start+30000,result)&&result==Code::WifiFail&&!s.calls);
 }
 // Shared nonce, schema and actual transport boundaries remain authentication concerns.
 nonce(auth);auto bad=auth.authenticateWifi("{}",2,0,security,7,185,false,true,0,evidence);assert(bad.code==Code::BadJson);
 slot.connection(7);assert(slot.reserve(bad,0));assert(slot.result(0,generation,result)&&result==Code::BadJson);slot.release(7);
 assert(auth.authenticateWifi(p.data(),p.size(),0,security,7,185,false,true,0,evidence).code==Code::NonceExpired);
 nonce(auth);assert(auth.authenticateWifi(p.data(),p.size(),0,security,7,uint16_t(p.size()+2),false,true,0,evidence).transportDenied);
 // OFF reservation and TIME dispatch are independent of the WIFI handoff slot.
 nonce(auth);accepted=auth.authenticateWifi(p.data(),p.size(),0,security,7,185,false,true,0,evidence);assert(slot.reserve(accepted,0));
 Dispatch dispatch;auto dg=dispatch.connect(1);Command t;t.op=Op::Time;t.unixSeconds=1793800000;
 assert(dispatch.reserve(dg,1,t,Code::Ok,0));Command off;off.op=Op::Off;assert(dispatch.reserve(dg,2,off,Code::Ok,0));Dispatch::Slot request;
 assert(dispatch.claim(request)&&request.command.op==Op::Off);dispatch.complete(request.ticket,Code::Ok);assert(dispatch.claim(request)&&request.command.op==Op::Time);
 binding.disconnect(7);binding.connect(peer,8);assert(!binding.captureWrite(peer,8));assert(!security.permitsWrite(8,evidence));
 std::cout<<"PASS: per-WRITE encrypted boundary shim/bond+generation, immediate ACK handoff, WIFI results/deadlines/storage ambiguity and OFF/TIME isolation\n";
}
