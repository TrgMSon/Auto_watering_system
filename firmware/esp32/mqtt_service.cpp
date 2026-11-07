#include "mqtt_service.h"
#ifdef ARDUINO
#include "mqtt_telemetry.h"
#include "mqtt_client_config.h"
#include "mqtt_config_nvs.h"
#include "mqtt_ingress.h"
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
Ingress ingress;
// Single MQTT worker scratch space; callbacks never access it. Keep payloads
// off the worker stack and separate from the callback-owned ingress FIFO.
struct WorkerBatch {
  char payloads[4][kCommandBytes];
  size_t sizes[4];
  uint64_t epochs[4],parsedEpochs[4];
  PumpCommand commands[4];
} workerBatch;
char commandTopic[64]={},ackTopic[64]={};
struct Lock{Lock(){portENTER_CRITICAL(&mux);}~Lock(){portEXIT_CRITICAL(&mux);}};
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
    {Lock lock; ingress.reset(); connectionGeneration_=commandConnection(false);}
    if(handle_&&!unhealthy_){
      if(esp_mqtt_client_stop(handle_)!=ESP_OK||esp_mqtt_client_destroy(handle_)!=ESP_OK)unhealthy_=true;
      else handle_=nullptr;
    }
    // A missing PUBACK alone does not prove the SDK outbox released the ACK.
    // Settle only after confirmed teardown; cleanup failure pins its record.
    if(!handle_&&ackFlight_>=0){commandAckSettled(ackSlot_,millis());ackFlight_=-1;}
    connected_.store(false);failed_.store(false);
  }
  bool connected()const override{return connected_.load();}
  bool failed()const override{return failed_.load();}
  void commands(){
    if(!usable())return;
    if(!subscribeStarted_.exchange(true)){
      const int id=esp_mqtt_client_subscribe(handle_,commandTopic,1);
      subscribeId_.store(id);
      subscribeAt_=millis();
      if(id<0)failed_.store(true);
      return;
    }
    if(!subscribed_.load()){
      const int sub=subackId_.load();
      if(sub>=0){
        if(sub!=subscribeId_.load()||!subackGranted_.load()){failed_.store(true);return;}
        subscribed_.store(true);
      }else {
        if(uint32_t(millis()-subscribeAt_)>=5000)failed_.store(true);
        return;
      }
    }
    // Copy at most four envelopes while locked. Parsing and batch submission
    // happen after releasing the ingress lock; connection/ON cancellation is
    // rechecked under the existing admission lock.
    size_t count=0;
    uint64_t generation;
    {
      Lock lock;
      if(!connected_.load())return;
      generation=connectionGeneration_;
      while(count<4&&ingress.pop(workerBatch.payloads[count],workerBatch.sizes[count],&workerBatch.epochs[count]))++count;
    }
    size_t parsed=0;
    for(size_t i=0;i<count;++i)
      if(parsePump(workerBatch.payloads[i],workerBatch.sizes[i],topics.identity,workerBatch.commands[parsed]))
        workerBatch.parsedEpochs[parsed++]=workerBatch.epochs[i];
    queueCommandBatch(workerBatch.commands,workerBatch.parsedEpochs,parsed,millis(),generation);
    if(ackFlight_>=0){
      bool published=false;
      {Lock lock;for(int id:pubacks_)if(id==ackFlight_)published=true;}
      if(published){ackAccepted(ackSlot_,millis());ackFlight_=-1;}
      else if(uint32_t(millis()-ackAt_)>=5000){failed_.store(true);return;}
      else return;
    }
    char bytes[320];int slot=-1;
    if(commandAck(millis(),slot,bytes,sizeof(bytes),topics.identity)){
      // enqueue is nonblocking; accepted means SDK outbox only, not backend receipt.
      const int id=esp_mqtt_client_enqueue(handle_,ackTopic,bytes,0,1,0,true);
      commandAckAttempt(slot,millis(),id>=0);
      if(id>=0){ackFlight_=id;ackSlot_=slot;ackAt_=millis();}
    }
  }
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
  static void event(void* arg,esp_event_base_t,int32_t id,void* data){
    auto& self=*static_cast<EspClient*>(arg);
    auto* e=static_cast<esp_mqtt_event_t*>(data);
    if(id==MQTT_EVENT_CONNECTED){
      Lock lock;ingress.reset();self.connectionGeneration_=commandConnection(true);
      self.subscribeStarted_.store(false);self.subscribed_.store(false);self.subscribeId_.store(-1);
      self.subackId_.store(-1);self.subackGranted_.store(false);
      for(auto& published:self.pubacks_)published=-1;
      self.pubackIndex_=0;
      self.connected_.store(true);
    }
    else if(id==MQTT_EVENT_DISCONNECTED||id==MQTT_EVENT_ERROR){
      Lock lock;ingress.reset();self.connectionGeneration_=commandConnection(false);
      self.subscribed_.store(false);self.connected_.store(false);self.failed_.store(true);
    }
    else if(id==MQTT_EVENT_SUBSCRIBED&&e){
      // SDK reports subscription failures using error_handle/return codes; require
      // exact matching message ID and granted QoS, never treat CONNACK as SUBACK.
      self.subackGranted_.store(e->data_len==1&&e->data&&
          (static_cast<unsigned char>(e->data[0])==0||static_cast<unsigned char>(e->data[0])==1));
      self.subackId_.store(e->msg_id);
    }
    else if(id==MQTT_EVENT_PUBLISHED&&e){
      Lock lock;self.pubacks_[self.pubackIndex_++%8]=e->msg_id;
    }
    else if(id==MQTT_EVENT_DATA&&e){
      Lock lock;
      if(self.connected_.load()&&self.subackGranted_.load()&&self.subackId_.load()>=0)
        ingress.fragment(e->topic,e->topic_len,e->data,e->data_len,e->total_data_len,e->current_data_offset,e->retain,commandTopic,commandIngressEpoch());
    }
  }
  esp_mqtt_client_handle_t handle_=nullptr;
  bool unhealthy_=false;
  uint64_t connectionGeneration_=0; // Protected by ingress mux, copied with envelopes.
  std::atomic<bool> connected_{false},failed_{false};
  std::atomic<bool> subscribeStarted_{false},subscribed_{false};
  std::atomic<int> subscribeId_{-1};
  std::atomic<int> subackId_{-1};
  int pubacks_[8]={-1,-1,-1,-1,-1,-1,-1,-1};
  unsigned pubackIndex_=0;
  std::atomic<bool> subackGranted_{false};
  uint32_t subscribeAt_=0,ackAt_=0;
  int ackFlight_=-1,ackSlot_=-1;
};
void worker(void*){
  // No secret/SDK error payload logging; some underlying logs include URIs.
  esp_log_level_set("mqtt_client",ESP_LOG_NONE);
  uint8_t mac[6]={};
  const bool ready=kTlsDateVerification&&esp_read_mac(mac,ESP_MAC_WIFI_STA)==ESP_OK&&topics.initialize(mac)&&loadConfig(mac,config)&&
      (kTlsNativeDates||prepareMqttTrust(config.ca));
  EspClient client;Runtime runtime(millis);
  snprintf(commandTopic,sizeof(commandTopic),"iot/garden/%s/cmd",topics.identity);
  snprintf(ackTopic,sizeof(ackTopic),"iot/garden/%s/ack",topics.identity);
  for(;;){
    sensors::Reading copy;bool have;
    {Lock lock;copy=latest;have=observed;}
    const time_t utc=time(nullptr);
    const bool clock=utc>=ble_commands::kUnixMin&&utc<=ble_commands::kUnixMax;
    runtime.service(millis(),ready,ble_wifi_service::networkSnapshot().online,clock,copy,have,client);
    client.commands();
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
