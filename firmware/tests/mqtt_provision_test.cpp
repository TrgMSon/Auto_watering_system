#include "../provisioning/mqtt_usb/protocol.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>
namespace mqtt_tls_shim {extern int parseResult;}
using namespace plant_mqtt;
namespace {
enum Type { U32,U16,Blob,String };
struct Entry {Type type;std::vector<uint8_t> bytes;bool operator==(const Entry& other)const{return type==other.type&&bytes==other.bytes;}};
using Data=std::map<std::string,Entry>;
struct PowerCut {};
Data live,flash;bool exists=false,immediate=false,after=false;unsigned count=0,failAt=0,cutAt=0,corruptAt=0;
std::vector<std::string> trace;
esp_err_t operation(const std::string& name,const std::function<esp_err_t()>& action){
  trace.push_back(name);++count;
  if(count==failAt&&!after)return 99;
  auto result=action();if(count==cutAt)throw PowerCut{};
  if(count==failAt&&after)return 99;
  return result;
}
esp_err_t get(const char* key,Type type,void* out,size_t* size){
  return operation(std::string("get:")+key,[&](){
    auto p=live.find(key);if(p==live.end())return ESP_ERR_NVS_NOT_FOUND;
    if(p->second.type!=type)return ESP_ERR_NVS_TYPE_MISMATCH;
    const auto& bytes=p->second.bytes;
    if(out&&*size<bytes.size())return ESP_ERR_NVS_INVALID_LENGTH;
    if(out)memcpy(out,bytes.data(),bytes.size());
    if(out&&count==corruptAt&&!bytes.empty())static_cast<uint8_t*>(out)[0]^=1;
    *size=bytes.size();return ESP_OK;
  });
}
esp_err_t set(const char* key,Type type,const void* p,size_t n){
  return operation(std::string("set:")+key,[&](){
    if(strcmp(key,"version")){
      // A candidate field must NEVER be written before durable invalidation.
      assert(flash.count("version")&&flash.at("version").type==U32);
      uint32_t v=1;memcpy(&v,flash.at("version").bytes.data(),sizeof(v));assert(v==0);
    }
    const auto* bytes=static_cast<const uint8_t*>(p);live[key]=Entry{type,std::vector<uint8_t>(bytes,bytes+n)};
    if(immediate)flash=live;
    return ESP_OK;
  });
}
void reset(const Data& initial=Data{}){live=flash=initial;exists=!initial.empty();count=failAt=cutAt=corruptAt=0;after=false;trace.clear();}
const uint8_t mac[]={2,3,4,5,6,7};
Config config(const char* host){
  Config c;strcpy(c.host,host);strcpy(c.ca,"-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----");
  strcpy(c.username,"fixture-mqtt-user");strcpy(c.password," fixture-MQTT-password+/= ");c.port=8883;return c;
}
void acceptable(const Config& old,const Config& candidate){
  Config read;count=failAt=cutAt=0;
  const bool ready=loadConfig(mac,read);
  if(ready)assert(sameConfig(read,old)||sameConfig(read,candidate));
  else assert(!read.password[0]&&!read.host[0]);
}
std::vector<uint8_t> encode(const Config& c){
  std::vector<uint8_t> bytes={'M','Q',1};bytes.insert(bytes.end(),mac,mac+6);
  bytes.push_back(uint8_t(c.port));bytes.push_back(uint8_t(c.port>>8));
  const char* fields[]={c.host,c.ca,c.username,c.password};
  for(auto field:fields){size_t n=strlen(field);bytes.push_back(uint8_t(n));bytes.push_back(uint8_t(n>>8));}
  for(auto field:fields)bytes.insert(bytes.end(),field,field+strlen(field));
  uint32_t crc=ble_keys::checksum(bytes.data(),bytes.size());for(unsigned i=0;i<4;++i)bytes.push_back(uint8_t(crc>>(8*i)));
  return bytes;
}
}
esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* h){
  assert(!strcmp(ns,"mqtt_cfg"));return operation(mode==NVS_READONLY?"open:ro":"open:rw",[&](){
    if(!exists&&mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
    exists=true;*h=1;return ESP_OK;
  });
}
void nvs_close(nvs_handle_t){}
esp_err_t nvs_get_u32(nvs_handle_t,const char* key,uint32_t* out){size_t size=4;return get(key,U32,out,&size);}
esp_err_t nvs_get_u16(nvs_handle_t,const char* key,uint16_t* out){size_t size=2;return get(key,U16,out,&size);}
esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* p,size_t* n){return get(key,Blob,p,n);}
esp_err_t nvs_get_str(nvs_handle_t,const char* key,char* p,size_t* n){return get(key,String,p,n);}
esp_err_t nvs_set_u32(nvs_handle_t,const char* key,uint32_t v){return set(key,U32,&v,4);}
esp_err_t nvs_set_u16(nvs_handle_t,const char* key,uint16_t v){return set(key,U16,&v,2);}
esp_err_t nvs_set_blob(nvs_handle_t,const char* key,const void* p,size_t n){return set(key,Blob,p,n);}
esp_err_t nvs_set_str(nvs_handle_t,const char* key,const char* p){return set(key,String,p,strlen(p)+1);}
esp_err_t nvs_commit(nvs_handle_t){return operation("commit",[](){flash=live;return ESP_OK;});}
int main(){
  const Config old=config("old.example.invalid");Config candidate=config("new.example.invalid");Config scratch,decoded;
  candidate.port=8884;strcpy(candidate.username,"new-fixture-user");strcpy(candidate.password,"new-fixture-password");
  strcpy(candidate.ca,"-----BEGIN CERTIFICATE-----\nnew-fixture\n-----END CERTIFICATE-----");
  auto record=encode(candidate);assert(mqtt_usb::decode(record.data(),record.size(),mac,decoded)&&sameConfig(candidate,decoded));
  const uint8_t wrong[]={2,3,4,5,6,8};assert(!mqtt_usb::decode(record.data(),record.size(),wrong,decoded));
  for(size_t cut=0;cut<record.size();++cut)assert(!mqtt_usb::decode(record.data(),cut,mac,decoded));
  for(size_t i=0;i<record.size();++i){record[i]^=1;assert(!mqtt_usb::decode(record.data(),record.size(),mac,decoded));record[i]^=1;}
  Config maximum=candidate;memset(maximum.host,'h',253);maximum.host[253]=0;
  memset(maximum.ca,'a',4095);maximum.ca[4095]=0;
  memcpy(maximum.ca,"-----BEGIN CERTIFICATE-----",27);memcpy(maximum.ca+4068,"-----END CERTIFICATE-----",25);
  memset(maximum.username,'u',63);maximum.username[63]=0;memset(maximum.password,'p',127);maximum.password[127]=0;
  auto largest=encode(maximum);assert(largest.size()==mqtt_usb::kRecordMax);
  assert(mqtt_usb::decode(largest.data(),largest.size(),mac,decoded)&&sameConfig(maximum,decoded));
  for(bool replace:{false,true}){
    mqtt_usb::Frame frame;std::vector<uint8_t> wire={'M','Q','P','1',uint8_t(replace),uint8_t(record.size()),uint8_t(record.size()>>8)};
    wire.insert(wire.end(),record.begin(),record.end());int result=0;
    for(uint8_t b:wire)result=frame.feed(b,UINT32_MAX-100);
    assert(result==1&&frame.replace()==replace&&frame.size()==record.size());
    assert(frame.feed(0,2)==-1);
  }
  mqtt_usb::Frame timeout;assert(!timeout.feed('M',UINT32_MAX-5));assert(timeout.feed('Q',9994)==-1);
  mqtt_usb::Frame wrongProtocol;assert(wrongProtocol.feed('B',0)==-1);
  mqtt_usb::Frame oversize;const size_t over=mqtt_usb::kRecordMax+1;int bad=0;
  for(uint8_t b:{uint8_t('M'),uint8_t('Q'),uint8_t('P'),uint8_t('1'),uint8_t(0),uint8_t(over),uint8_t(over>>8)})bad=oversize.feed(b,0);
  assert(bad==-1);
  Config invalid=candidate;strcpy(invalid.host,"ssl://invalid");auto invalidRecord=encode(invalid);
  assert(!mqtt_usb::decode(invalidRecord.data(),invalidRecord.size(),mac,decoded));
  reset();assert(mqtt_usb::store(invalid,mac,false,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Invalid&&count==0);
  reset();assert(mqtt_usb::store(old,mac,false,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Verified);
  const Data initial=flash;Config read;assert(loadConfig(mac,read)&&sameConfig(read,old));
  unsigned previous=count;assert(mqtt_usb::store(candidate,mac,false,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Exists);
  assert(count==previous+1&&flash==initial); // default refusal has no write
  for(int parseResult:{-1,1}){
    mqtt_tls_shim::parseResult=parseResult;previous=count;
    assert(mqtt_usb::store(candidate,mac,true,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Invalid&&count==previous);
  }
  mqtt_tls_shim::parseResult=0;
  for(bool mode:{false,true}){
    immediate=mode;reset(initial);
    assert(mqtt_usb::store(candidate,mac,true,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Verified);
    const unsigned steps=count;const auto successfulTrace=trace;assert(loadConfig(mac,read)&&sameConfig(read,candidate));
    for(unsigned step=1;step<=steps;++step){
      for(bool late:{false,true}){
        reset(initial);failAt=step;after=late;
        auto outcome=mqtt_usb::store(candidate,mac,true,mqtt_usb::parseCertificate,scratch);
        assert(outcome!=mqtt_usb::Result::Verified);acceptable(old,candidate);
        live=flash;acceptable(old,candidate); // reboot: only durable storage survives
      }
      reset(initial);cutAt=step;bool cut=false;
      try{mqtt_usb::store(candidate,mac,true,mqtt_usb::parseCertificate,scratch);}catch(const PowerCut&){cut=true;}
      assert(cut);live=flash;acceptable(old,candidate);
    }
    for(unsigned i=0;i<successfulTrace.size();++i)if(successfulTrace[i]=="get:password"){
      reset(initial);corruptAt=i+1;
      assert(mqtt_usb::store(candidate,mac,true,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Unconfirmed);
      acceptable(old,candidate);live=flash;acceptable(old,candidate);
    }
  }
  // Production reader rejects missing marker/fields, wrong NVS types, bounds,
  // bad host/PEM envelope, embedded NUL and identity mismatch.
  for(const char* key:{"version","sta_mac","port","host","ca","username","password"}){
    reset(initial);live.erase(key);assert(!loadConfig(mac,read)&&!read.password[0]);
    reset(initial);live[key].type=live[key].type==String?Blob:String;assert(!loadConfig(mac,read));
  }
  reset(initial);live["host"].bytes.assign(255,'h');assert(!loadConfig(mac,read));
  reset(initial);live["password"].bytes={'a',0,'b',0};assert(!loadConfig(mac,read));
  reset(initial);live["version"].bytes={0,0,0,0};assert(!loadConfig(mac,read));
  assert(mqtt_usb::store(candidate,mac,false,mqtt_usb::parseCertificate,scratch)==mqtt_usb::Result::Exists);
  reset(initial);assert(!loadConfig(wrong,read));
  reset(initial);live["host"].bytes={'s','s','l',':','/',0};assert(!loadConfig(mac,read));
  std::cout<<"PASS actual MQTT writer/reader/frame: typed roundtrip, replace, failures and interruptions at each storage step (staged/immediate persistence), no accepted mixed config; SDK shims only\n";
}
