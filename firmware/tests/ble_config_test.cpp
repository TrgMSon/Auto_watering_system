#include "../esp32/ble_auth.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
using namespace ble_commands;
namespace {
const char fixture[]="test-ble-secret-key";
const char knownNonce[]="84a9f21b3c0e5a1d";
const char vectorSig[]="1e4b0b9e4f4d0ea5";
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
std::vector<std::string> fields={"\"min\":30.0","\"max\":70.0","\"auto\":1","\"max_dur\":600","\"min_water\":5.0",std::string("\"sig\":\"")+vectorSig+"\""};
std::string json(const std::vector<std::string>& f) { std::string s="{"; for(const auto& x:f) { if(s.size()>1)s+=",";s+=x; } return s+"}"; }
std::string changed(unsigned i,const std::string& value) { auto f=fields; f[i]=f[i].substr(0,f[i].find(':')+1)+value; return json(f); }
Code parseString(const std::string& p,ConfigRequest& r) { return parseConfig(p.data(),p.size(),r); }
void nonce(Authenticator& a,uint32_t now=0) { const char* p; uint16_t n; assert(a.readNonce(0,now,p,n)&&n==16&&!memcmp(p,knownNonce,n)); }
ConfigAuthentication send(Authenticator& a,const std::string& p,uint32_t now=0) {return a.authenticateConfig(p.data(),p.size(),now);}
}
int main() {
 const std::string canonical="84a9f21b3c0e5a1d|CONFIG|30.0|70.0|1|600|5.0";
 assert(sign(fixture,canonical)==vectorSig); // Independent OpenSSL, not firmware canonical helper.
 Keys keys;HostCrypto crypto;Authenticator auth(crypto);auth.loadKey(&keys);
 ConfigRequest r;const auto payload=json(fields);
 assert(parseString(payload,r)==Code::Ok && r.minPercentTenths==300 && r.maxPercentTenths==700 && r.minWaterCmTenths==50);
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
 for(const char* v:{"0","0.0","0.1","429496729.5"}) assert(parseString(changed(4,v),r)==Code::Ok);
 // Integer JSON values canonicalize exactly to .0, no float conversion/rounding.
 assert(parseString(changed(0,"30"),r)==Code::Ok);configCanonical(knownNonce,r,buffer,sizeof(buffer));assert(buffer==canonical);
 auto maximum=fields;maximum[0]="\"min\":99.9";maximum[1]="\"max\":100.0";maximum[4]="\"min_water\":429496729.5";
 const auto compactMax=json(maximum);assert(compactMax.size()==96 && parseString(compactMax,r)==Code::Ok);
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
 nonce(auth);assert(send(auth,padded+" ").code==Code::BadJson);assert(send(auth,payload).code==Code::NonceExpired);
 for(uint32_t start:{0u,UINT32_MAX-10}) {nonce(auth,start);assert(send(auth,payload,start+29999).code==Code::Ok);nonce(auth,start);assert(send(auth,payload,start+30000).code==Code::NonceExpired);}
 nonce(auth);auth.reset();assert(send(auth,payload).code==Code::NonceExpired);
 assert(authenticatedWrite(2)&&authenticatedWrite(7)&&!authenticatedWrite(3)&&!authenticatedWrite(6));
 // ConfigRequest has no owner Command or result op; parse/auth cannot apply it.
 Command command;assert(parse(payload.data(),payload.size(),command)==Code::BadJson);
 std::cout<<"PASS: CONFIG independent HMAC/vector/raw UTF-8, exact tenths/schema/ranges/bounds/permutations/shared nonce; endpoint closed\n";
}
