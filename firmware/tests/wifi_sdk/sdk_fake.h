#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <vector>
#include <string>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_ERR_INVALID_STATE=1,ESP_ERR_WIFI_NOT_STARTED=2,ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED=3,ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED=4,ESP_ERR_NVS_NOT_FOUND=5;
using esp_event_base_t=const char*;
using esp_event_handler_instance_t=void*;
using Handler=void(*)(void*,esp_event_base_t,int32_t,void*);
constexpr int ESP_EVENT_ANY_ID=-1;
extern const char *WIFI_EVENT,*IP_EVENT;
#define ESP_EVENT_DEFINE_BASE(name) const char* name=#name
struct esp_netif_t{};
struct Addr{uint32_t addr=0;};
struct esp_netif_ip_info_t{Addr ip,netmask,gw;};
struct wifi_init_config_t{int nvs_enable=1;};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
constexpr int WIFI_STORAGE_RAM=0,WIFI_MODE_STA=1,WIFI_IF_STA=0,WIFI_AUTH_WPA2_PSK=3,WIFI_PROTOCOL_11B=1,WIFI_PROTOCOL_11G=2,WIFI_PROTOCOL_11N=4;
struct wifi_config_t{struct {uint8_t ssid[32]={},password[64]={};struct {int authmode=0;}threshold;uint8_t failure_retry_cnt=0;}sta;};
struct wifi_ap_record_t{uint8_t ssid[33]={},bssid[6]={};uint8_t primary=1;int authmode=WIFI_AUTH_WPA2_PSK;};
using nvs_handle_t=int;constexpr int NVS_READONLY=0,NVS_READWRITE=1;
namespace shim {
struct Registration{esp_event_base_t base;int id;Handler handler;void* arg;};
extern std::vector<Registration> handlers;
extern std::vector<std::string> calls;
extern std::vector<uint8_t> record;
extern std::string fail;
extern bool netifExists,associated,readFail;
extern uint32_t posted;
extern wifi_config_t config;
extern wifi_ap_record_t ap;
extern esp_netif_ip_info_t ip;
int api(const char* name);
void emit(esp_event_base_t base,int id=0,void* data=nullptr);
void reset();
}
esp_err_t esp_netif_init();esp_err_t esp_event_loop_create_default();
esp_netif_t* esp_netif_get_handle_from_ifkey(const char*);
esp_netif_t* esp_netif_create_default_wifi_sta();void esp_netif_destroy_default_wifi(esp_netif_t*);
esp_err_t esp_wifi_init(const wifi_init_config_t*);esp_err_t esp_wifi_deinit();esp_err_t esp_wifi_stop();
esp_err_t esp_wifi_set_storage(int);esp_err_t esp_wifi_set_mode(int);
esp_err_t esp_event_handler_instance_register(esp_event_base_t,int,Handler,void*,esp_event_handler_instance_t*);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t,int,esp_event_handler_instance_t);
esp_err_t esp_event_post(esp_event_base_t,int,const void*,size_t,uint32_t);
esp_err_t esp_netif_dhcpc_stop(esp_netif_t*);esp_err_t esp_netif_dhcpc_start(esp_netif_t*);
esp_err_t esp_netif_set_ip_info(esp_netif_t*,const esp_netif_ip_info_t*);esp_err_t esp_netif_get_ip_info(esp_netif_t*,esp_netif_ip_info_t*);
esp_err_t esp_wifi_set_protocol(int,int);esp_err_t esp_wifi_set_config(int,const wifi_config_t*);
esp_err_t esp_wifi_start();esp_err_t esp_wifi_connect();esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*);
esp_err_t nvs_open(const char*,int,nvs_handle_t*);void nvs_close(nvs_handle_t);
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);esp_err_t nvs_commit(nvs_handle_t);

constexpr int ESP_LOG_NONE=0; void esp_log_level_set(const char*,int);
