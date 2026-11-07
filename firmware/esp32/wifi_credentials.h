#pragma once
#include "ble_wifi_attempt.h"
#include "ble_key_record.h"
namespace ble_wifi {
constexpr size_t kCredentialMax=110;
enum class Read { Ok, Missing, Error };
enum class Write { Committed, NotWritten, Unknown };
struct Blob {
  virtual ~Blob() {}
  virtual Read read(uint8_t* bytes,size_t capacity,size_t& size)=0;
  virtual Write writeCommit(const uint8_t* bytes,size_t size)=0;
};
inline size_t encodeCredentials(const ble_commands::WifiRequest& r,const uint8_t* mac,uint8_t* out) {
  if(!ble_commands::validWifiRequest(r))return 0;
  size_t s=strlen(r.ssid),p=strlen(r.password);
  out[0]='W';out[1]='F';out[2]=1;memcpy(out+3,mac,6);out[9]=uint8_t(s);out[10]=uint8_t(p);
  memcpy(out+11,r.ssid,s);memcpy(out+11+s,r.password,p);
  size_t n=11+s+p;uint32_t crc=ble_keys::checksum(out,n);
  for(unsigned i=0;i<4;++i)out[n+i]=uint8_t(crc>>(8*i));
  return n+4;
}
inline bool decodeCredentials(const uint8_t* bytes,size_t n,const uint8_t* mac,ble_commands::WifiRequest& r) {
  r.clear();
  if(n<24||n>kCredentialMax||bytes[0]!='W'||bytes[1]!='F'||bytes[2]!=1||memcmp(bytes+3,mac,6))return false;
  size_t s=bytes[9],p=bytes[10];
  if(!s||s>32||p<8||p>63||n!=15+s+p)return false;
  uint32_t crc=ble_keys::checksum(bytes,n-4);
  for(unsigned i=0;i<4;++i)if(bytes[n-4+i]!=uint8_t(crc>>(8*i)))return false;
  // Embedded NUL must not silently turn into a shorter credential.
  if(memchr(bytes+11,0,s+p))return false;
  memcpy(r.ssid,bytes+11,s);memcpy(r.password,bytes+11+s,p);
  if(!ble_commands::validWifiRequest(r)){r.clear();return false;}return true;
}
class CredentialStorage : public Storage {
 public:
  CredentialStorage(Blob& blob,const uint8_t* mac):blob_(blob){memcpy(mac_,mac,6);}
  Read load(ble_commands::WifiRequest& r) {
    r.clear();uint8_t bytes[kCredentialMax]={};size_t n=0;
    Read result=blob_.read(bytes,sizeof(bytes),n);
    if(result==Read::Ok&&!decodeCredentials(bytes,n,mac_,r))result=Read::Error;
    ble_commands::wipeWifi(bytes,sizeof(bytes));return result;
  }
  Persistence commitVerified(const ble_commands::WifiRequest& r) override {
    uint8_t bytes[kCredentialMax]={},back[kCredentialMax]={};
    const size_t n=encodeCredentials(r,mac_,bytes);size_t actual=0;
    Persistence result=Persistence::NotCommitted;
    if(n){
      Write w=blob_.writeCommit(bytes,n);
      if(w!=Write::NotWritten){
        result=Persistence::Unconfirmed;
        if(w==Write::Committed && blob_.read(back,sizeof(back),actual)==Read::Ok &&
            actual==n && !memcmp(bytes,back,n)) {
          ble_commands::WifiRequest decoded;
          if(decodeCredentials(back,actual,mac_,decoded))result=Persistence::Verified;
        }
      }
    }
    ble_commands::wipeWifi(bytes,sizeof(bytes));ble_commands::wipeWifi(back,sizeof(back));return result;
  }
 private:
  Blob& blob_;uint8_t mac_[6];
};
}
