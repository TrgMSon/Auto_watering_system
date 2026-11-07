#pragma once
#include "../../esp32/mqtt_config_nvs.h"
#include "../../esp32/ble_key_record.h"
#include <mbedtls/x509_crt.h>
namespace mqtt_usb {
constexpr size_t kRecordMax=4561; // 19-byte header + max fields + CRC32
// USB-only record: MQ, version1, MAC6, port LE16, four LE16 lengths,
// host/CA/username/password bytes, CRC32 IEEE LE32. Not an NVS blob/schema.
inline bool decode(const uint8_t* bytes,size_t n,const uint8_t (&mac)[6],plant_mqtt::Config& out){
  ble_commands::wipeWifi(&out,sizeof(out));
  if(!bytes||n<23||n>kRecordMax||bytes[0]!='M'||bytes[1]!='Q'||bytes[2]!=1||memcmp(bytes+3,mac,6))return false;
  const uint32_t crc=ble_keys::checksum(bytes,n-4);
  for(unsigned i=0;i<4;++i)if(bytes[n-4+i]!=uint8_t(crc>>(8*i)))return false;
  out.port=uint16_t(bytes[9])|(uint16_t(bytes[10])<<8);
  char* fields[]={out.host,out.ca,out.username,out.password};
  const size_t caps[]={sizeof(out.host),sizeof(out.ca),sizeof(out.username),sizeof(out.password)};
  size_t offset=19;
  for(unsigned i=0;i<4;++i){
    const size_t length=size_t(bytes[11+2*i])+(size_t(bytes[12+2*i])<<8);
    if(!length||length>=caps[i]||length>n-4-offset||memchr(bytes+offset,0,length)){
      ble_commands::wipeWifi(&out,sizeof(out));return false;
    }
    memcpy(fields[i],bytes+offset,length);offset+=length;
  }
  if(offset!=n-4||!plant_mqtt::configured(out)){ble_commands::wipeWifi(&out,sizeof(out));return false;}
  return true;
}
class Frame {
 public:
  ~Frame(){clear();}
  bool expired(uint32_t now)const{return started_&&uint32_t(now-start_)>=10000;}
  int feed(uint8_t byte,uint32_t now){
    if(failed_||done_||expired(now))return fail();
    if(!started_){started_=true;start_=now;}
    if(used_<7){
      header_[used_++]=byte;
      if(used_<=4&&byte!=uint8_t("MQP1"[used_-1]))return fail();
      if(used_==5&&byte>1)return fail();
      if(used_==7){expected_=size_t(header_[5])+(size_t(header_[6])<<8);if(expected_<23||expected_>sizeof(record))return fail();}
      return 0;
    }
    record[used_++-7]=byte;
    if(used_==expected_+7){done_=true;return 1;}return 0;
  }
  bool replace()const{return header_[4]==1;}
  size_t size()const{return expected_;}
  void clear(){ble_commands::wipeWifi(record,sizeof(record));ble_commands::wipeWifi(header_,sizeof(header_));}
  uint8_t record[kRecordMax]={};
 private:
  int fail(){clear();failed_=true;return -1;}
  uint8_t header_[7]={};size_t used_=0,expected_=0;uint32_t start_=0;
  bool started_=false,done_=false,failed_=false;
};
enum class Result { Verified, Invalid, Exists, NotWritten, Unconfirmed };
inline bool parseCertificate(const char* pem){
  mbedtls_x509_crt chain;mbedtls_x509_crt_init(&chain);
  const bool ok=mbedtls_x509_crt_parse(&chain,reinterpret_cast<const unsigned char*>(pem),strlen(pem)+1)==0&&chain.raw.p&&chain.raw.len;
  mbedtls_x509_crt_free(&chain);return ok; // parse only, not server trust/hostname
}
inline Result store(const plant_mqtt::Config& candidate,const uint8_t (&mac)[6],bool replace,
    bool (*certificateValid)(const char*),plant_mqtt::Config& scratch){
  ble_commands::wipeWifi(&scratch,sizeof(scratch));
  if(!plant_mqtt::configured(candidate)||!certificateValid||!certificateValid(candidate.ca))return Result::Invalid;
  // Any existing namespace (including incomplete/invalid config) needs explicit
  // replacement. Unknown storage errors do not authorize overwriting.
  nvs_handle_t h;esp_err_t e=nvs_open(plant_mqtt::kConfigNamespace,NVS_READONLY,&h);
  if(e==ESP_OK){nvs_close(h);if(!replace)return Result::Exists;}
  else if(e!=ESP_ERR_NVS_NOT_FOUND)return Result::NotWritten;
  if(nvs_open(plant_mqtt::kConfigNamespace,NVS_READWRITE,&h)!=ESP_OK)return Result::NotWritten;
  Result result=Result::Unconfirmed;uint32_t marker=1;
  // No candidate fields until version=0 was committed AND read back. Any
  // failure after the first set is conservative: never promise old credentials.
  if(nvs_set_u32(h,"version",0)==ESP_OK&&nvs_commit(h)==ESP_OK&&
      nvs_get_u32(h,"version",&marker)==ESP_OK&&marker==0&&
      nvs_set_blob(h,"sta_mac",mac,6)==ESP_OK&&nvs_set_u16(h,"port",candidate.port)==ESP_OK&&
      nvs_set_str(h,"host",candidate.host)==ESP_OK&&nvs_set_str(h,"ca",candidate.ca)==ESP_OK&&
      nvs_set_str(h,"username",candidate.username)==ESP_OK&&nvs_set_str(h,"password",candidate.password)==ESP_OK&&
      nvs_commit(h)==ESP_OK&&nvs_get_u32(h,"version",&marker)==ESP_OK&&marker==0&&
      plant_mqtt::readConfigFields(h,mac,scratch)&&plant_mqtt::sameConfig(candidate,scratch)&&
      nvs_set_u32(h,"version",1)==ESP_OK&&nvs_commit(h)==ESP_OK&&
      plant_mqtt::readReadyConfig(h,mac,scratch)&&plant_mqtt::sameConfig(candidate,scratch))result=Result::Verified;
  nvs_close(h);ble_commands::wipeWifi(&scratch,sizeof(scratch));return result;
}
}
