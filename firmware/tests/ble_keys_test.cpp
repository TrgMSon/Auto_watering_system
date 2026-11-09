#include "../provisioning/ble_key_usb/protocol.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
using namespace ble_keys;
struct Memory : provisioning::Writer {
  std::vector<uint8_t> data; bool readFail=false, writeFail=false, corruptWrite=false;
  Read read(uint8_t* p,size_t cap,size_t& n) override {
    n=0; if(readFail) return Read::Error;
    if(data.empty()) return Read::Missing;
    if(data.size()>cap) return Read::Error;
    n=data.size(); memcpy(p,data.data(),n); return Read::Ok;
  }
  bool write(const uint8_t* p,size_t n) override {
    if(writeFail) return false;
    data.assign(p,p+n); if(corruptWrite) data.back()^=1; return true;
  }
};
struct Crypto : ble_commands::Crypto {
  bool random(uint8_t (&p)[8]) override { memset(p,0,8); return true; }
  bool hmac(const uint8_t* k,size_t n,const char* m,size_t len,uint8_t (&out)[32]) override {
    unsigned size=0; return HMAC(EVP_sha256(),k,int(n),reinterpret_cast<const uint8_t*>(m),len,out,&size) && size==32;
  }
};
int main() {
  uint8_t mac[6]={2,3,4,5,6,7}, wrong[6]={2,3,4,5,6,8};
  const uint8_t fixture[]={' ',0xc3,0xa9,'+','/','=',' ',0};
  uint8_t record[kRecordMax]={}, out[128]={};
  size_t n=encode(record,mac,fixture,sizeof(fixture)), size=0;
  assert(valid(record,n,mac)); assert(!valid(record,n,wrong));
  Memory s; KeySource keys;
  assert(keys.initialize(s,mac)==Status::Missing && !keys.load(out,sizeof(out),size));
  assert(provisioning::store(s,record,n,mac,false)==provisioning::Result::Ok);
  assert(keys.initialize(s,mac)==Status::Loaded && keys.load(out,sizeof(out),size));
  assert(size==sizeof(fixture) && !memcmp(out,fixture,size));
  assert(provisioning::store(s,record,n,mac,false)==provisioning::Result::Exists);
  assert(provisioning::store(s,record,n,mac,true)==provisioning::Result::Ok);
  assert(provisioning::store(s,record,n,wrong,true)==provisioning::Result::Invalid);
  for(size_t cut=0;cut<n;++cut) assert(!valid(record,cut,mac));
  for(size_t i=0;i<n;++i) { record[i]^=1; assert(!valid(record,n,mac)); record[i]^=1; }
  for(uint8_t v : {uint8_t(0),uint8_t(129)}) { record[9]=v; assert(!valid(record,n,mac)); }
  record[9]=sizeof(fixture); record[2]=2; assert(!valid(record,n,mac)); record[2]=1;
  s.readFail=true; assert(keys.initialize(s,mac)==Status::Invalid);
  assert(!keys.load(out,sizeof(out),size));
  assert(provisioning::store(s,record,n,mac,true)==provisioning::Result::StorageError);
  s.readFail=false; s.writeFail=true;
  assert(provisioning::store(s,record,n,mac,true)==provisioning::Result::StorageError);
  s.writeFail=false; s.corruptWrite=true;
  assert(provisioning::store(s,record,n,mac,true)==provisioning::Result::StorageError);
  assert(keys.initialize(s,mac)==Status::Invalid);
  s.corruptWrite=false; s.write(record,n); assert(keys.initialize(s,wrong)==Status::Invalid);
  assert(keys.initialize(s,mac)==Status::Loaded);
  Crypto crypto; ble_commands::Authenticator auth(crypto); auth.loadKey(&keys);
  const char* nonce; uint16_t length; assert(auth.readNonce(0,0,nonce,length));
  std::string canonical=std::string(nonce,length)+"|PUMP_OFF|0";
  uint8_t digest[32]; assert(crypto.hmac(fixture,sizeof(fixture),canonical.data(),canonical.size(),digest));
  std::string sig; const char hex[]="0123456789abcdef";
  for(unsigned i=0;i<8;++i) { sig+=hex[digest[i]>>4]; sig+=hex[digest[i]&15]; }
  std::string payload="{\"cmd\":\"PUMP_OFF\",\"dur\":0,\"sig\":\""+sig+"\"}";
  assert(auth.authenticate(payload.data(),payload.size(),0).code==ble_commands::Code::Ok);
  assert(auth.readNonce(0,0,nonce,length));
  s.data[kHeader]^=1;
  // A different, valid provisioned key must reject the old signature.
  uint8_t other[]={1,2,3}; n=encode(record,mac,other,sizeof(other)); s.write(record,n);
  keys.initialize(s,mac); auth.loadKey(&keys); auth.readNonce(0,0,nonce,length);
  assert(auth.authenticate(payload.data(),payload.size(),0).code==ble_commands::Code::BadSig);
  s.data.clear(); keys.initialize(s,mac); auth.loadKey(&keys); auth.readNonce(0,0,nonce,length);
  assert(auth.authenticate(payload.data(),payload.size(),0).missingKey);
  std::vector<uint8_t> frame={'B','K','P','1',0,uint8_t(n),0}; frame.insert(frame.end(),record,record+n);
  provisioning::Frame f; int result=0;
  for(auto b:frame) result=f.feed(b,10);
  assert(result==1 && f.size()==n && !f.replace());
  provisioning::Frame timeout; assert(timeout.feed('B',0)==0);
  assert(!timeout.expired(9999) && timeout.expired(10000)); assert(timeout.feed('K',10000)==-1);
  provisioning::Frame bad; assert(bad.feed('X',0)==-1);
  for(unsigned index : {4u,5u,6u}) {
    provisioning::Frame invalid; auto changed=frame; changed[index]=255;
    int r=0; for(size_t i=0;i<7 && r==0;++i) r=invalid.feed(changed[i],0);
    assert(r==-1);
  }
  uint8_t maxKey[128]={}; n=encode(record,mac,maxKey,128); assert(valid(record,n,mac));
  assert(!encode(record,mac,maxKey,0) && !encode(record,mac,maxKey,129));
  std::cout << "BLE key record/provisioning/authentication PASS\n";
}
