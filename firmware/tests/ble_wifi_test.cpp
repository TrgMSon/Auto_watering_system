#include "pump/Arduino.h"
#include "../esp32/ble_wifi_attempt.h"
#include "../esp32/ble_dispatch.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
using namespace ble_commands;
uint32_t fakeNow=0;int outputLevel=-1,writes=0,outputModes=0;
namespace {
const char key[]="test-ble-secret-key";
const char nonceValue[]="84a9f21b3c0e5a1d";
struct Keys:KeySource {bool load(uint8_t* p,size_t cap,size_t& n)const override{n=sizeof(key)-1;if(cap<n)return false;memcpy(p,key,n);return true;}};
struct CryptoImpl:Crypto{
 bool fail=false;
 bool random(uint8_t (&p)[8])override{const uint8_t b[]={0x84,0xa9,0xf2,0x1b,0x3c,0x0e,0x5a,0x1d};memcpy(p,b,8);return !fail;}
 bool hmac(const uint8_t* k,size_t n,const char* m,size_t len,uint8_t (&p)[32])override{unsigned size=0;return !fail&&HMAC(EVP_sha256(),k,int(n),reinterpret_cast<const uint8_t*>(m),len,p,&size)&&size==32;}
};
std::string sign(const std::string& canonical){uint8_t d[32];unsigned n=0;assert(HMAC(EVP_sha256(),key,sizeof(key)-1,reinterpret_cast<const uint8_t*>(canonical.data()),canonical.size(),d,&n));const char h[]="0123456789abcdef";std::string s;for(unsigned i=0;i<8;++i){s+=h[d[i]>>4];s+=h[d[i]&15];}return s;}
std::string payload(const std::string& ssid="HomeWiFi",const std::string& pass="12345678",const std::string& sig="ca81265e275b9a4a"){
 return "{\"ssid\":\""+ssid+"\",\"pass\":\""+pass+"\",\"sig\":\""+sig+"\"}";
}
void nonce(Authenticator& a,uint32_t now=0){const char* p;uint16_t n;assert(a.readNonce(0,now,p,n));}
struct Radio:ble_wifi::Radio{
 State status=State::Pending;bool startOk=true;unsigned starts=0,stops=0;WifiRequest copy;
 bool startRam(const WifiRequest& c)override{++starts;copy=c;return startOk;}
 State poll()override{return status;}
 void stopCandidate()override{++stops;copy.clear();}
};
struct Store:ble_wifi::Storage{
 std::string old="previous fixture record";unsigned calls=0;bool writeFail=false,readFail=false;
 ble_wifi::Persistence commitVerified(const WifiRequest& c)override{++calls;if(writeFail)return ble_wifi::Persistence::NotCommitted;old=std::string(c.ssid)+":"+c.password;if(readFail)return ble_wifi::Persistence::Unconfirmed;return ble_wifi::Persistence::Verified;}
};
}
int main(){
 assert(sign("84a9f21b3c0e5a1d|WIFI|HomeWiFi|12345678")=="ca81265e275b9a4a");
 CryptoImpl crypto;Keys keys;Authenticator auth(crypto);auth.loadKey(&keys);WifiSecurity security;security.connect(1);
 const auto p=payload();
 // Untrusted input cannot even be dereferenced by an unmet gate.
 auto inaccessible=reinterpret_cast<const char*>(uintptr_t(1));
 nonce(auth);auto denied=auth.authenticateWifi(inaccessible,p.size(),0,security,1,185);assert(denied.securityDenied&&!denied.request.password[0]);
 security.update(1,true,true,false);assert(!security.permits(1));security.update(1,true,false,true);assert(!security.permits(1));
 security.update(1,true,true,true);assert(security.permits(1));
 assert(auth.authenticateWifi(p.data(),p.size(),0,security,1,185).code==Code::NonceExpired);
 nonce(auth);auto accepted=auth.authenticateWifi(p.data(),p.size(),0,security,1,185);assert(accepted.code==Code::Ok);
 nonce(auth);assert(auth.authenticateWifi(p.data(),p.size(),0,security,1,uint16_t(p.size()+3)).code==Code::Ok);
 nonce(auth);assert(auth.authenticateWifi(inaccessible,p.size(),0,security,1,uint16_t(p.size()+2)).transportDenied);
 nonce(auth);assert(auth.authenticateWifi(inaccessible,p.size(),0,security,1,185,true).transportDenied);
 nonce(auth);assert(auth.authenticateWifi(inaccessible,p.size(),0,security,1,185,false,false).transportDenied);
 nonce(auth);assert(auth.authenticateWifi(inaccessible,p.size(),0,security,1,185,false,true,1).transportDenied);
 security.disconnect(1);security.connect(2);security.update(1,true,true,true);assert(!security.permits(2));
 nonce(auth);assert(auth.authenticateWifi(inaccessible,p.size(),0,security,1,185).securityDenied);
 security.update(2,true,true,true);security.update(2,false,false,false);assert(!security.permits(2));security.update(2,true,true,true);
 nonce(auth);accepted=auth.authenticateWifi(p.data(),p.size(),0,security,2,185);assert(accepted.code==Code::Ok);
 const auto wrong=payload("HomeWiFi","12345678","0000000000000000");nonce(auth);
 auto bad=auth.authenticateWifi(wrong.data(),wrong.size(),0,security,2,185);assert(bad.code==Code::BadSig&&!bad.request.password[0]);
 auth.loadKey(nullptr);nonce(auth);assert(auth.authenticateWifi(p.data(),p.size(),0,security,2,185).missingKey);auth.loadKey(&keys);
 nonce(auth);crypto.fail=true;assert(auth.authenticateWifi(p.data(),p.size(),0,security,2,185).missingKey);crypto.fail=false;
 WifiRequest request;char canonical[kWifiCanonicalCapacity];
 // Escapes decode before signing; literal UTF-8 and Unicode escape are identical.
 const std::string ssid="Caf\xc3\xa9\xf0\x9f\x8c\xb1";
 const std::string pass="p\xc3\xa4ss\\\"word";
 const std::string expected=std::string(nonceValue)+"|WIFI|"+ssid+"|"+pass;
 const auto escaped=payload("Caf\\u00e9\\ud83c\\udf31","p\\u00e4ss\\\\\\\"word",sign(expected));
 assert(parseWifi(escaped.data(),escaped.size(),request)==Code::Ok&&!strcmp(request.ssid,ssid.c_str())&&!strcmp(request.password,pass.c_str()));
 assert(wifiCanonical(nonceValue,request,canonical,sizeof(canonical))==expected.size()&&canonical==expected);
 nonce(auth);assert(auth.authenticateWifi(escaped.data(),escaped.size(),0,security,2,185).code==Code::Ok);
 for(const auto& text:{payload("a|b"),payload("a\\u007cb"),payload("ok","1234\\u007c678")})assert(parseWifi(text.data(),text.size(),request)==Code::OutOfRange);
 for(const auto& text:{payload("\\ud800"),payload("\\udc00"),payload("\\ud800\\u0041"),payload("\\u0000"),payload("\\x20"),payload(std::string("x\xc0\x80",3)),payload(std::string("\xed\xa0\x80",3)),payload(std::string("\xf4\x90\x80\x80",4)),payload(std::string("\xe2\x82",2)),payload(std::string(33,'s')),payload("ok",std::string(64,'p')),p+"x",p.substr(0,p.size()-1)})assert(parseWifi(text.data(),text.size(),request)==Code::BadJson);
 for(const auto& text:{payload(""),payload("ok","short")})assert(parseWifi(text.data(),text.size(),request)==Code::OutOfRange);
 for(const char* text:{"{}","{\"ssid\":1,\"pass\":\"12345678\",\"sig\":\"ca81265e275b9a4a\"}","{\"ssid\":\"a\",\"ssid\":\"a\",\"pass\":\"12345678\"}","{\"ssid\":\"a\",\"pass\":\"12345678\",\"unknown\":0}","{\"ssid\":[],\"pass\":{},\"sig\":null}"})assert(parseWifi(text,strlen(text),request)==Code::BadJson);
 auto max=payload(std::string(32,'s'),std::string(63,'p'));assert(parseWifi(max.data(),max.size(),request)==Code::Ok);
 assert(wifiCanonical(nonceValue,request,canonical,sizeof(canonical))==118);assert(!wifiCanonical(nonceValue,request,canonical,118));
 auto padded=p+std::string(kWifiPayloadMax-p.size(),' ');assert(parseWifi(padded.data(),padded.size(),request)==Code::Ok);
 assert(parseWifi(padded.data(),padded.size()+1,request)==Code::BadJson); // length reject before any read
 nonce(auth);assert(auth.authenticateWifi(padded.data(),padded.size(),0,security,2,185).transportDenied);
 // Byte counts, not Unicode character counts; bounded nonterminated input structs.
 std::string utf8ssid;for(unsigned i=0;i<16;++i)utf8ssid+="\xc3\xa9";
 auto unicode32=payload(utf8ssid);assert(parseWifi(unicode32.data(),unicode32.size(),request)==Code::Ok);
 auto unicode34=payload(utf8ssid+"\xc3\xa9");assert(parseWifi(unicode34.data(),unicode34.size(),request)==Code::BadJson);
 memset(request.ssid,'x',sizeof(request.ssid));assert(!wifiCanonical(nonceValue,request,canonical,sizeof(canonical)));
 // Shared nonce lifecycle across all four command families.
 const std::string pump="{\"cmd\":\"PUMP_OFF\",\"dur\":0,\"sig\":\"238041d3721ef169\"}";
 const std::string time="{\"unix\":1793800000,\"sig\":\"26b87998f4bc4778\"}";
 const std::string config="{\"min\":30.0,\"max\":70.0,\"auto\":1,\"max_dur\":600,\"min_water\":5.0,\"sig\":\"1e4b0b9e4f4d0ea5\"}";
 for(unsigned other=0;other<3;++other){auto call=[&](){return other==0?auth.authenticate(pump.data(),pump.size(),0).code:other==1?auth.authenticate(time.data(),time.size(),0,Op::Time).code:auth.authenticateConfig(config.data(),config.size(),0).code;};
  nonce(auth);assert(auth.authenticateWifi(p.data(),p.size(),0,security,2,185).code==Code::Ok);assert(call()==Code::NonceExpired);
  nonce(auth);assert(call()==Code::Ok);assert(auth.authenticateWifi(p.data(),p.size(),0,security,2,185).code==Code::NonceExpired);
 }
 nonce(auth);assert(auth.authenticateWifi("{}",2,0,security,2,185).code==Code::BadJson);assert(auth.authenticateConfig(config.data(),config.size(),0).code==Code::NonceExpired);
 for(uint32_t start:{0u,UINT32_MAX-50u}){nonce(auth,start);assert(auth.authenticateWifi(p.data(),p.size(),start+29999,security,2,185).code==Code::Ok);nonce(auth,start);assert(auth.authenticateWifi(p.data(),p.size(),start+30000,security,2,185).code==Code::NonceExpired);}
 // Attempt engine only uses fakes: no host Wi-Fi, no real credentials or storage.
 using Attempt=ble_wifi::Attempt;Code code;
 for(uint32_t start:{0u,UINT32_MAX-100u}){
  Radio radio;Store store;Attempt attempt;
  assert(attempt.reserve(2,accepted,true,false,185,start));assert(!attempt.reserve(2,accepted,true,false,185,start));
  attempt.service(start,2,radio,store,security);assert(radio.starts==1&&store.calls==0);
  attempt.service(start+3000,2,radio,store,security);assert(attempt.state()==Attempt::State::Connecting&&!attempt.result(2,start+3000,code));
  attempt.service(start+29999,2,radio,store,security);assert(store.calls==0);
  radio.status=Radio::State::CandidateIp;attempt.service(start+30000,2,radio,store,security);
  assert(attempt.result(2,start+30000,code)&&code==Code::WifiFail&&store.calls==0&&store.old=="previous fixture record");
 }
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,2,radio,store,security);radio.status=Radio::State::CandidateIp;attempt.service(29999,2,radio,store,security);
  assert(store.calls==1&&attempt.result(2,29999,code)&&code==Code::WifiOk);assert(attempt.result(2,32998,code));assert(!attempt.result(2,32999,code));}
 for(unsigned fail=0;fail<3;++fail){Radio radio;Store store;Attempt attempt;store.writeFail=fail==0;store.readFail=fail==1;radio.startOk=fail!=2;
  assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,2,radio,store,security);radio.status=Radio::State::CandidateIp;attempt.service(1,2,radio,store,security);
  assert(attempt.result(2,1,code)&&code==Code::WifiFail);
  assert(fail==1 ? store.old!="previous fixture record" && attempt.persistence()==ble_wifi::Persistence::Unconfirmed : store.old=="previous fixture record");}
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,3,radio,store,security);assert(!radio.starts&&attempt.state()==Attempt::State::Idle);
  assert(!attempt.reserve(3,accepted,true,false,185,0));assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(1000,2,radio,store,security);assert(!radio.starts);}
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,2,radio,store,security);
  radio.status=Radio::State::CandidateIp;attempt.service(5000,3,radio,store,security);assert(store.calls==1&&!attempt.result(3,5000,code)&&attempt.state()==Attempt::State::Idle);}
 {Radio radio;Store store;Attempt attempt;assert(!attempt.reserve(2,accepted,false,false,185,0));assert(!attempt.reserve(2,accepted,true,true,185,0));assert(!attempt.reserve(2,accepted,true,false,23,0));}
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));
  security.update(2,false,false,false);attempt.service(0,2,radio,store,security);
  assert(!radio.starts&&store.calls==0&&attempt.state()==Attempt::State::Idle);security.update(2,true,true,true);}
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,2,radio,store,security);
  radio.status=Radio::State::Failed;attempt.service(1,2,radio,store,security);
  assert(attempt.result(2,1,code)&&code==Code::WifiFail&&!store.calls&&store.old=="previous fixture record");}
 // Separate attempt capacity does not touch the dispatch OFF reservation.
 {Radio radio;Store store;Attempt attempt;assert(attempt.reserve(2,accepted,true,false,185,0));attempt.service(0,2,radio,store,security);
  Dispatch dispatch;auto gen=dispatch.connect(1);Command on;on.op=Op::On;on.seconds=5;for(unsigned i=0;i<3;++i)assert(dispatch.reserve(gen,i,on,Code::Ok,0));Command off;off.op=Op::Off;assert(dispatch.reserve(gen,4,off,Code::Ok,0));Dispatch::Slot slot;assert(dispatch.claim(slot)&&slot.command.op==Op::Off);}
 char result[64];assert(Attempt::resultJson(Code::WifiOk,result,sizeof(result))&&std::string(result)=="{\"op\":\"WIFI\",\"code\":\"WIFI_OK\"}");assert(!Attempt::resultJson(Code::Ok,result,sizeof(result)));
 assert(!authenticatedWrite(6)&&!authenticatedWrite(3));
 std::cout<<"PASS: WIFI parser/auth/security/generation/MTU/UTF-8, shared nonce, bounded attempt/deadline/storage failure/OFF isolation (fakes only)\n";
}
