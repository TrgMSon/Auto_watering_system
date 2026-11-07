#include "sdk_fake.h"
#include <cassert>
#include <algorithm>
const char *WIFI_EVENT="wifi",*IP_EVENT="ip";
namespace shim {
std::vector<Registration> handlers;std::vector<std::string> calls;std::vector<uint8_t> record;
std::string fail;bool netifExists=false,associated=false,readFail=false;uint32_t posted=0;
wifi_config_t config;wifi_ap_record_t ap;esp_netif_ip_info_t ip;
int api(const char* name){calls.emplace_back(name);return fail==name?99:ESP_OK;}
void emit(esp_event_base_t base,int id,void* data){auto copy=handlers;for(auto& h:copy)if(h.base==base&&(h.id==-1||h.id==id))h.handler(h.arg,base,id,data);}
void reset(){assert(handlers.empty());calls.clear();record.clear();fail.clear();netifExists=associated=readFail=false;posted=0;config={};ap={};ip={};}
}
static esp_netif_t netif;
esp_err_t esp_netif_init(){return shim::api("netif_init");}esp_err_t esp_event_loop_create_default(){return shim::api("loop_init");}
esp_netif_t* esp_netif_get_handle_from_ifkey(const char*){return shim::netifExists?&netif:nullptr;}
esp_netif_t* esp_netif_create_default_wifi_sta(){if(shim::api("netif_create"))return nullptr;shim::netifExists=true;return &netif;}
void esp_netif_destroy_default_wifi(esp_netif_t*){shim::api("netif_destroy");shim::netifExists=false;}
esp_err_t esp_wifi_init(const wifi_init_config_t* c){assert(c->nvs_enable==0);return shim::api("wifi_init");}esp_err_t esp_wifi_deinit(){return shim::api("deinit");}
esp_err_t esp_wifi_stop(){int e=shim::api("stop");if(!e)shim::associated=false;return e;}
esp_err_t esp_wifi_set_storage(int mode){assert(mode==WIFI_STORAGE_RAM);return shim::api("RAM");}
esp_err_t esp_wifi_set_mode(int mode){assert(mode==WIFI_MODE_STA);return shim::api("mode");}
esp_err_t esp_event_handler_instance_register(esp_event_base_t base,int id,Handler h,void* arg,esp_event_handler_instance_t* out){if(shim::api("register"))return 99;shim::handlers.push_back({base,id,h,arg});*out=reinterpret_cast<void*>(uintptr_t(shim::handlers.size()));return 0;}
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base,int,esp_event_handler_instance_t){shim::api("unregister");auto& h=shim::handlers;h.erase(std::remove_if(h.begin(),h.end(),[&](const shim::Registration& r){return r.base==base;}),h.end());return 0;}
esp_err_t esp_event_post(esp_event_base_t,int,const void* data,size_t n,uint32_t timeout){assert(n==4&&timeout==0);memcpy(&shim::posted,data,4);return shim::api("barrier");}
esp_err_t esp_netif_dhcpc_stop(esp_netif_t*){return shim::api("dhcp_stop");}esp_err_t esp_netif_dhcpc_start(esp_netif_t*){return shim::api("dhcp_start");}
esp_err_t esp_netif_set_ip_info(esp_netif_t*,const esp_netif_ip_info_t* p){int e=shim::api("clear_ip");if(!e)shim::ip=*p;return e;}
esp_err_t esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t* p){int e=shim::api("get_ip");if(!e)*p=shim::ip;return e;}
esp_err_t esp_wifi_set_protocol(int,int){return shim::api("protocol");}
esp_err_t esp_wifi_set_config(int,const wifi_config_t* p){int e=shim::api("config");if(!e)shim::config=*p;return e;}
esp_err_t esp_wifi_start(){return shim::api("start");}esp_err_t esp_wifi_connect(){return shim::api("connect");}
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t* p){int e=shim::api("association");if(e)return e;if(!shim::associated)return 99;*p=shim::ap;return 0;}
esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* h){assert(std::string(ns)=="wifi_prov");*h=1;int e=shim::api(mode==NVS_READONLY?"open_read":"open_write");if(e)return e;return mode==NVS_READONLY&&shim::record.empty()?ESP_ERR_NVS_NOT_FOUND:0;}
void nvs_close(nvs_handle_t){shim::api("close");}
esp_err_t nvs_get_blob(nvs_handle_t,const char* key,void* out,size_t* n){assert(std::string(key)=="record");int e=shim::api("read");if(e||shim::readFail)return 99;if(!out){*n=shim::record.size();return 0;}if(*n<shim::record.size())return 99;*n=shim::record.size();memcpy(out,shim::record.data(),*n);return 0;}
esp_err_t nvs_set_blob(nvs_handle_t,const char* key,const void* p,size_t n){assert(std::string(key)=="record");shim::record.assign(static_cast<const uint8_t*>(p),static_cast<const uint8_t*>(p)+n);return shim::api("write");}
esp_err_t nvs_commit(nvs_handle_t){return shim::api("commit");}

void esp_log_level_set(const char*,int level){assert(level==ESP_LOG_NONE);}
