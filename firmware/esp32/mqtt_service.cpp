#include "mqtt_service.h"
#ifdef ARDUINO
#include "mqtt_telemetry.h"
#include "mqtt_client_config.h"
#include "ble_wifi_service.h"
#include <Arduino.h>
#include <mqtt_client.h>
#include <nvs.h>
#include <esp_mac.h>
#include <esp_log.h>
#include <time.h>
#include <atomic>
namespace plant_mqtt {
namespace {
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
sensors::Reading latest;bool observed=false,started=false;
Config config;Topics topics;
struct Lock{Lock(){portENTER_CRITICAL(&mux);}~Lock(){portEXIT_CRITICAL(&mux);}};
template<size_t N> bool readString(nvs_handle_t h,const char* key,char (&out)[N]){
  size_t size=N;return nvs_get_str(h,key,out,&size)==ESP_OK&&size>1&&size<=N&&out[size-1]==0;
}
bool load(const uint8_t (&mac)[6]){
  nvs_handle_t h;if(nvs_open("mqtt_cfg",NVS_READONLY,&h)!=ESP_OK)return false;
  uint8_t identity[6]={};size_t size=sizeof(identity);uint32_t version=0;
  const bool ok=nvs_get_u32(h,"version",&version)==ESP_OK&&version==1&&
      nvs_get_blob(h,"sta_mac",identity,&size)==ESP_OK&&size==6&&!memcmp(identity,mac,6)&&
      nvs_get_u16(h,"port",&config.port)==ESP_OK&&readString(h,"host",config.host)&&
      readString(h,"ca",config.ca)&&readString(h,"username",config.username)&&readString(h,"password",config.password)&&configured(config);
  nvs_close(h);
  if(!ok)ble_commands::wipeWifi(&config,sizeof(config));
  return ok;
}
class EspClient:public Client {
 public:
  bool start()override{
    if(handle_||unhealthy_)return false;
    connected_.store(false);failed_.store(false);
    esp_mqtt_client_config_t c={};
    if(!clientConfig(config,topics,c))return false;
    handle_=esp_mqtt_client_init(&c);
    if(!handle_)return false;
    if(esp_mqtt_client_register_event(handle_,MQTT_EVENT_ANY,event,this)!=ESP_OK||esp_mqtt_client_start(handle_)!=ESP_OK){
      esp_mqtt_client_destroy(handle_);handle_=nullptr;return false;
    }
    return true;
  }
  void stop()override{
    if(handle_&&!unhealthy_){
      if(esp_mqtt_client_stop(handle_)!=ESP_OK||esp_mqtt_client_destroy(handle_)!=ESP_OK)unhealthy_=true;
      else handle_=nullptr;
    }
    connected_.store(false);failed_.store(false);
  }
  bool connected()const override{return connected_.load();}
  bool failed()const override{return failed_.load();}
  bool status()override{
    if(!usable()||esp_mqtt_client_get_outbox_size(handle_)!=0)return false;
    return esp_mqtt_client_publish(handle_,topics.status,kActive,0,1,1)>=0;
  }
  bool sample(const sensors::Reading& r,bool have,uint32_t)override{
    // Status publish may have blocked: freshness uses the actual serialization time.
    char bytes[256];size_t n=telemetry(topics,r,have,millis(),bytes,sizeof(bytes));
    return n&&usable()&&esp_mqtt_client_publish(handle_,topics.telemetry,bytes,int(n),0,0)>=0;
  }
 private:
  bool usable()const{
    // Recheck after potentially blocking previous calls. Do not initiate a
    // publish if Wi-Fi runtime reports offline/provisioning at this check.
    return handle_&&connected_.load()&&ble_wifi_service::networkSnapshot().online;
  }
  static void event(void* arg,esp_event_base_t,int32_t id,void*){
    auto& self=*static_cast<EspClient*>(arg);
    if(id==MQTT_EVENT_CONNECTED)self.connected_.store(true);
    else if(id==MQTT_EVENT_DISCONNECTED||id==MQTT_EVENT_ERROR){self.connected_.store(false);self.failed_.store(true);}
  }
  esp_mqtt_client_handle_t handle_=nullptr;
  bool unhealthy_=false;
  std::atomic<bool> connected_{false},failed_{false};
};
void worker(void*){
  // No secret/SDK error payload logging; some underlying logs include URIs.
  esp_log_level_set("mqtt_client",ESP_LOG_NONE);
  uint8_t mac[6]={};
  const bool ready=kTlsDateVerification&&esp_read_mac(mac,ESP_MAC_WIFI_STA)==ESP_OK&&topics.initialize(mac)&&load(mac)&&
      (kTlsNativeDates||prepareMqttTrust(config.ca));
  EspClient client;Runtime runtime(millis);
  for(;;){
    sensors::Reading copy;bool have;
    {Lock lock;copy=latest;have=observed;}
    const time_t utc=time(nullptr);
    const bool clock=utc>=ble_commands::kUnixMin&&utc<=ble_commands::kUnixMax;
    runtime.service(millis(),ready,ble_wifi_service::networkSnapshot().online,clock,copy,have,client);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}
bool begin(){
  if(!kTlsDateVerification)return false;
  if(started)return false;
  started=true;
  return xTaskCreate(worker,"mqtt-telemetry",6144,nullptr,1,nullptr)==pdPASS;
}
void publish(const sensors::Reading& reading){Lock lock;latest=reading;observed=true;}
}
#endif
