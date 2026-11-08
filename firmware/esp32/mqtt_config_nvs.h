#pragma once
#include "mqtt_telemetry.h"
#include "ble_wifi_request.h"
#include <nvs.h>
namespace plant_mqtt {
constexpr const char* kConfigNamespace="mqtt_cfg";
template<size_t N> inline bool configString(nvs_handle_t h,const char* key,char (&out)[N]){
  size_t size=N;
  return nvs_get_str(h,key,out,&size)==ESP_OK&&size>1&&size<=N&&out[size-1]==0&&strlen(out)+1==size;
}
inline bool readConfigFields(nvs_handle_t h,const uint8_t (&mac)[6],Config& out){
  ble_commands::wipeWifi(&out,sizeof(out));
  uint8_t identity[6]={};size_t size=sizeof(identity);
  const bool ok=nvs_get_blob(h,"sta_mac",identity,&size)==ESP_OK&&size==6&&!memcmp(identity,mac,6)&&
      nvs_get_u16(h,"port",&out.port)==ESP_OK&&configString(h,"host",out.host)&&
      configString(h,"ca",out.ca)&&configString(h,"username",out.username)&&configString(h,"password",out.password)&&configured(out);
  if(!ok)ble_commands::wipeWifi(&out,sizeof(out));
  return ok;
}
inline bool readReadyConfig(nvs_handle_t h,const uint8_t (&mac)[6],Config& out){
  ble_commands::wipeWifi(&out,sizeof(out));uint32_t version=0;
  return nvs_get_u32(h,"version",&version)==ESP_OK&&version==1&&readConfigFields(h,mac,out);
}
inline bool loadConfig(const uint8_t (&mac)[6],Config& out){
  ble_commands::wipeWifi(&out,sizeof(out));nvs_handle_t h;
  if(nvs_open(kConfigNamespace,NVS_READONLY,&h)!=ESP_OK)return false;
  const bool ok=readReadyConfig(h,mac,out);nvs_close(h);return ok;
}
inline bool sameConfig(const Config& a,const Config& b){
  return a.port==b.port&&!strcmp(a.host,b.host)&&!strcmp(a.ca,b.ca)&&
      !strcmp(a.username,b.username)&&!strcmp(a.password,b.password);
}
}
