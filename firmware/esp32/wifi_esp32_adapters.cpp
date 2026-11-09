#include "wifi_esp32_adapters.h"
#ifdef ARDUINO
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <nvs.h>
#include <esp_log.h>
ESP_EVENT_DEFINE_BASE(WIFI_PROVISION_BARRIER);
namespace ble_wifi {
namespace {
constexpr const char* ns="wifi_prov";constexpr const char* record="record";
uint32_t networkValue(uint32_t address){
  const uint8_t* p=reinterpret_cast<const uint8_t*>(&address);
  return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];
}
bool usableIp(const esp_netif_ip_info_t& info){
  const uint32_t ip=networkValue(info.ip.addr),mask=networkValue(info.netmask.addr),hosts=~mask;
  const unsigned first=ip>>24;
  return first>0&&first<224&&first!=127&&hosts>=3&&mask!=0&&
      (hosts&(hosts+1))==0&&(ip&hosts)!=0&&(ip&hosts)!=hosts;
}
}
void EspRadioSdk::event(void* arg,esp_event_base_t base,int32_t,void* data){
  auto& self=*static_cast<EspRadioSdk*>(arg);
  if(self.count_.fetch_add(1)>=32)self.fault_.store(true);
  if(base==WIFI_PROVISION_BARRIER&&data){uint32_t token;memcpy(&token,data,sizeof(token));self.ack_.store(token);}
}
bool EspRadioSdk::cleanup(){
  bool ok=true;
  if(wifiOwned_){if(!stop())ok=false;if(esp_wifi_deinit()!=ESP_OK)ok=false;wifiOwned_=false;}
  if(wifiHandler_){if(esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiHandler_)!=ESP_OK)ok=false;wifiHandler_=nullptr;}
  if(ipHandler_){if(esp_event_handler_instance_unregister(IP_EVENT,ESP_EVENT_ANY_ID,ipHandler_)!=ESP_OK)ok=false;ipHandler_=nullptr;}
  if(barrierHandler_){if(esp_event_handler_instance_unregister(WIFI_PROVISION_BARRIER,0,barrierHandler_)!=ESP_OK)ok=false;barrierHandler_=nullptr;}
  if(netif_){esp_netif_destroy_default_wifi(netif_);netif_=nullptr;}
  ready_=false;return ok;
}
EspRadioSdk::~EspRadioSdk(){if(!cleanup())fault_.store(true);}
bool EspRadioSdk::initialize(){
  if(ready_)return true;
  if(failed_)return false;
  failed_=true; // Any partially failed initialization requires reconstruction/reboot.
  // Driver INFO/debug output can expose SSID. Our adapter never logs credentials.
  esp_log_level_set("wifi",ESP_LOG_NONE);
  esp_log_level_set("wifi_init",ESP_LOG_NONE);
  if(esp_netif_init()!=ESP_OK)return false;
  esp_err_t e=esp_event_loop_create_default();
  if(e!=ESP_OK&&e!=ESP_ERR_INVALID_STATE)return false;
  // Exclusive ownership of the STA Wi-Fi driver; do not attach to another manager.
  if(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))return false;
  netif_=esp_netif_create_default_wifi_sta();
  if(!netif_)return false;
  wifi_init_config_t config=WIFI_INIT_CONFIG_DEFAULT();
  config.nvs_enable=0; // SDK must not load/save its own credential configuration.
  if(esp_wifi_init(&config)!=ESP_OK){cleanup();return false;}
  wifiOwned_=true;
  if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||
      esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,this,&wifiHandler_)!=ESP_OK||
      esp_event_handler_instance_register(IP_EVENT,ESP_EVENT_ANY_ID,event,this,&ipHandler_)!=ESP_OK||
      esp_event_handler_instance_register(WIFI_PROVISION_BARRIER,0,event,this,&barrierHandler_)!=ESP_OK){cleanup();return false;}
  ready_=true;failed_=false;return true;
}
bool EspRadioSdk::stop(){
  if(!wifiOwned_)return false;
  esp_err_t e=esp_wifi_stop();return e==ESP_OK||e==ESP_ERR_WIFI_NOT_STARTED;
}
bool EspRadioSdk::barrier(uint32_t token){
  return esp_event_post(WIFI_PROVISION_BARRIER,0,&token,sizeof(token),0)==ESP_OK;
}
bool EspRadioSdk::drain(uint32_t& token){
  count_.exchange(0);token=ack_.load();return !fault_.load();
}
bool EspRadioSdk::clearIp(){
  if(!ready_)return false;
  esp_err_t e=esp_netif_dhcpc_stop(netif_);
  if(e!=ESP_OK&&e!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)return false;
  esp_netif_ip_info_t zero={},actual={};
  if(esp_netif_set_ip_info(netif_,&zero)!=ESP_OK||esp_netif_get_ip_info(netif_,&actual)!=ESP_OK||actual.ip.addr)return false;
  // Restore DHCP's INIT/STARTED lifecycle after explicitly clearing old state.
  e=esp_netif_dhcpc_start(netif_);
  return e==ESP_OK||e==ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED;
}
bool EspRadioSdk::configureRam(const ble_commands::WifiRequest& r){
  if(!ready_||!ble_commands::validWifiRequest(r))return false;
  wifi_config_t config={};
  memcpy(config.sta.ssid,r.ssid,strlen(r.ssid)); // exactly 32 bytes need no terminator in IDF array
  memcpy(config.sta.password,r.password,strlen(r.password));
  config.sta.threshold.authmode=WIFI_AUTH_WPA2_PSK;
  config.sta.failure_retry_cnt=0;
  const bool ok=esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK&&
      esp_wifi_set_protocol(WIFI_IF_STA,WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N)==ESP_OK&&
      esp_wifi_set_config(WIFI_IF_STA,&config)==ESP_OK;
  ble_commands::wipeWifi(&config,sizeof(config));return ok;
}
bool EspRadioSdk::startConnect(){return esp_wifi_start()==ESP_OK&&esp_wifi_connect()==ESP_OK;}
bool EspRadioSdk::candidateIp(const ble_commands::WifiRequest& r){
  ipv4_=0;
  wifi_ap_record_t ap={},after={};esp_netif_ip_info_t ip={};
  if(!ready_||fault_.load()||esp_wifi_sta_get_ap_info(&ap)!=ESP_OK||
      esp_netif_get_ip_info(netif_,&ip)!=ESP_OK||!usableIp(ip)||
      esp_wifi_sta_get_ap_info(&after)!=ESP_OK||memcmp(ap.bssid,after.bssid,6)||memcmp(ap.ssid,after.ssid,33))return false;
  const size_t length=strlen(r.ssid);
  const bool same=memcmp(ap.ssid,r.ssid,length)==0 && (length==32||ap.ssid[length]==0);
  // ESP32 target is 2.4GHz-only; channel and negotiated auth are checked too.
  const bool ok=!fault_.load()&&same&&ap.primary>=1&&ap.primary<=14&&ap.authmode==WIFI_AUTH_WPA2_PSK;
  if(ok)ipv4_=ip.ip.addr;
  return ok;
}
Read NvsCredentialBlob::read(uint8_t* out,size_t capacity,size_t& size){
  size=0;nvs_handle_t h;esp_err_t e=nvs_open(ns,NVS_READONLY,&h);
  if(e==ESP_ERR_NVS_NOT_FOUND)return Read::Missing;
  if(e!=ESP_OK)return Read::Error;
  size_t n=0;e=nvs_get_blob(h,record,nullptr,&n);
  if(e==ESP_ERR_NVS_NOT_FOUND){nvs_close(h);return Read::Missing;}
  if(e!=ESP_OK||!n||n>capacity){nvs_close(h);return Read::Error;}
  e=nvs_get_blob(h,record,out,&n);nvs_close(h);
  if(e!=ESP_OK)return Read::Error;
  size=n;return Read::Ok;
}
Write NvsCredentialBlob::writeCommit(const uint8_t* bytes,size_t n){
  if(!n||n>kCredentialMax)return Write::NotWritten;
  nvs_handle_t h;if(nvs_open(ns,NVS_READWRITE,&h)!=ESP_OK)return Write::NotWritten;
  const bool ok=nvs_set_blob(h,record,bytes,n)==ESP_OK&&nvs_commit(h)==ESP_OK;
  nvs_close(h);return ok?Write::Committed:Write::Unknown;
}
}
#endif
