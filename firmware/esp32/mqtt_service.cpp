#include "mqtt_service.h"
#include "device_identity.h"
#ifdef ARDUINO
#include "mqtt_telemetry.h"
#include "mqtt_client_config.h"
#include "mqtt_config_nvs.h"
#include "mqtt_ingress.h"
#include "ble_wifi_service.h"
#include <Arduino.h>
#include <mqtt_client.h>
#include <nvs.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <time.h>
#include <atomic>
namespace plant_mqtt {
namespace {
constexpr char kLogTag[]="mqtt_client";
constexpr uint32_t kWorkerStackBytes=6144;
constexpr uint32_t kHeapLogIntervalMs=60000;
constexpr uint32_t kInternalHeapCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
::telemetry_snapshot::Snapshot latest;bool observed=false,started=false;
std::atomic<bool> brokerConnected{false};
Config config;Topics topics;
Ingress ingress,configIngress;
Subscriptions subscriptions;
int fragmentTopic=-1; // MQTT delivers one contiguous fragmented message at a time.
// One reusable payload scratch buffer; parsed commands stay batched for atomic
// connection-generation validation. Callbacks never access this worker storage.
struct WorkerBatch {
  char payload[kCommandBytes];
  uint64_t epochs[4],parsedEpochs[4];
  PumpCommand commands[4];
} workerBatch;
char commandTopic[64]={},configTopic[64]={},ackTopic[64]={};
struct Lock{Lock(){portENTER_CRITICAL(&mux);}~Lock(){portEXIT_CRITICAL(&mux);}};
void logInternalHeap(){
  const size_t freeBytes=heap_caps_get_free_size(kInternalHeapCaps);
  const size_t minimumBytes=heap_caps_get_minimum_free_size(kInternalHeapCaps);
  const size_t largestBlock=heap_caps_get_largest_free_block(kInternalHeapCaps);
  Serial.printf("[RAM] internal_free=%luB minimum_free=%luB largest_block=%luB\n",
      static_cast<unsigned long>(freeBytes),static_cast<unsigned long>(minimumBytes),
      static_cast<unsigned long>(largestBlock));
}
class EspClient:public Client {
 public:
  bool start()override{
    if(handle_||unhealthy_)return false;
    connected_.store(false);failed_.store(false);
    esp_mqtt_client_config_t c={};
    if(!clientConfig(config,topics,c)){
      Serial.println("[MQTT] broker/TLS configuration is invalid");
      return false;
    }
    Serial.println("[MQTT] allocating client; heap snapshot follows");
    logInternalHeap();
    handle_=esp_mqtt_client_init(&c);
    if(!handle_){
      Serial.println("[MQTT] client initialization failed (check internal heap)");
      return false;
    }
    const esp_err_t registerResult=esp_mqtt_client_register_event(handle_,MQTT_EVENT_ANY,event,this);
    if(registerResult!=ESP_OK){
      Serial.printf("[MQTT] event registration failed err=0x%x\n",static_cast<unsigned>(registerResult));
      esp_mqtt_client_destroy(handle_);handle_=nullptr;return false;
    }
    const esp_err_t startResult=esp_mqtt_client_start(handle_);
    if(startResult!=ESP_OK){
      Serial.printf("[MQTT] client start failed err=0x%x\n",static_cast<unsigned>(startResult));
      esp_mqtt_client_destroy(handle_);handle_=nullptr;return false;
    }
    Serial.println("[MQTT] client started; waiting for transport/TLS result");
    return true;
  }
  void stop()override{
    brokerConnected.store(false);
    {Lock lock; ingress.reset();configIngress.reset();fragmentTopic=-1; connectionGeneration_=commandConnection(false);}
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
      {Lock lock;subscriptions.requested(0,id);}
      const int configId=esp_mqtt_client_subscribe(handle_,configTopic,1);
      {Lock lock;subscriptions.requested(1,configId);}
      Serial.printf("[MQTT] subscribe requested cmd_id=%d config_id=%d\n",id,configId);
      subscribeAt_=millis();
      if(id<0||configId<0){
        Serial.println("[MQTT] subscribe request failed locally");
        failed_.store(true);
      }
      return;
    }
    if(!subscribed_.load()){
      int pumpState,configState;
      {Lock lock;pumpState=subscriptions.state(0);configState=subscriptions.state(1);}
      if(pumpState<0||configState<0){
        Serial.printf("[MQTT] broker rejected subscription cmd=%d config=%d\n",pumpState,configState);
        failed_.store(true);return;
      }
      if(pumpState==1&&configState==1){
        subscribed_.store(true);
      }else {
        if(uint32_t(millis()-subscribeAt_)>=5000){
          Serial.printf("[MQTT] SUBACK timeout cmd=%d config=%d\n",pumpState,configState);
          failed_.store(true);return;
        }
        // Do not hold pump/OFF work behind a slow config SUBACK.
        if(pumpState!=1)return;
      }
    }
    // Keep pump and config FIFOs independent and preserve batch admission while
    // reusing one bounded payload buffer instead of four copies per FIFO.
    drain(ingress,false);
    drain(configIngress,true);
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
    char bytes[kActiveCapacity];
    const size_t size = activeStatus(bytes, sizeof(bytes));
    return size && esp_mqtt_client_publish(handle_,topics.status,bytes,int(size),1,1)>=0;
  }
  bool sample(const ::telemetry_snapshot::Snapshot& r,bool have,uint32_t)override{
    // Status publish may have blocked: freshness uses the actual serialization time.
    char bytes[256];size_t n=telemetry(topics,r,have,millis(),bytes,sizeof(bytes));
    return n&&usable()&&esp_mqtt_client_publish(handle_,topics.telemetry,bytes,int(n),0,0)>=0;
  }
 private:
  void drain(Ingress& source,bool configuration){
    uint64_t generation;
    {
      Lock lock;
      if(!connected_.load())return;
      generation=connectionGeneration_;
    }
    size_t parsed=0;
    for(size_t i=0;i<4;++i){
      size_t size=0;
      uint64_t epoch=0;
      {
        Lock lock;
        if(!connected_.load()||connectionGeneration_!=generation)return;
        if(!source.pop(workerBatch.payload,size,&epoch))break;
      }
      const bool valid=configuration
          ?parseConfiguration(workerBatch.payload,size,topics.identity,workerBatch.commands[parsed])
          :parsePump(workerBatch.payload,size,topics.identity,workerBatch.commands[parsed]);
      if(valid)workerBatch.parsedEpochs[parsed++]=epoch;
    }
    queueCommandBatch(workerBatch.commands,workerBatch.parsedEpochs,parsed,millis(),generation);
  }
  bool usable()const{
    // Recheck after potentially blocking previous calls. Do not initiate a
    // publish if Wi-Fi runtime reports offline/provisioning at this check.
    return handle_&&connected_.load()&&ble_wifi_service::networkSnapshot().online;
  }
  static void event(void* arg,esp_event_base_t,int32_t id,void* data){
    auto& self=*static_cast<EspClient*>(arg);
    auto* e=static_cast<esp_mqtt_event_t*>(data);
    if(id==MQTT_EVENT_CONNECTED){
      {
        Lock lock;ingress.reset();configIngress.reset();fragmentTopic=-1;subscriptions.reset();self.connectionGeneration_=commandConnection(true);
        self.subscribeStarted_.store(false);self.subscribed_.store(false);
        for(auto& published:self.pubacks_)published=-1;
        self.pubackIndex_=0;
        self.connected_.store(true);
        brokerConnected.store(true);
      }
      Serial.println("[MQTT] connected with verified TLS");
    }
    else if(id==MQTT_EVENT_DISCONNECTED||id==MQTT_EVENT_ERROR){
      {
        Lock lock;ingress.reset();configIngress.reset();fragmentTopic=-1;subscriptions.reset();self.connectionGeneration_=commandConnection(false);
        self.subscribed_.store(false);self.connected_.store(false);self.failed_.store(true);
        brokerConnected.store(false);
      }
      if(id==MQTT_EVENT_ERROR){
        if(e&&e->error_handle){
          const esp_mqtt_error_codes_t& error=*e->error_handle;
          Serial.printf("[MQTT] error detail type=%d esp_err=0x%x tls_err=0x%x cert_flags=0x%x broker_rc=%d\n",
              static_cast<int>(error.error_type),static_cast<unsigned>(error.esp_tls_last_esp_err),
              static_cast<unsigned>(error.esp_tls_stack_err),static_cast<unsigned>(error.esp_tls_cert_verify_flags),
              static_cast<int>(error.connect_return_code));
        }else Serial.println("[MQTT] error event has no SDK error details");
        logInternalHeap();
        Serial.println("[MQTT] TLS/transport or broker connection error; reconnect scheduled");
      }else Serial.println("[MQTT] disconnected; reconnect scheduled");
    }
    else if(id==MQTT_EVENT_SUBSCRIBED&&e){
      // SDK reports subscription failures using error_handle/return codes; require
      // exact matching message ID and granted QoS, never treat CONNACK as SUBACK.
      const bool granted=e->data_len==1&&e->data&&
          (static_cast<unsigned char>(e->data[0])==0||static_cast<unsigned char>(e->data[0])==1);
      Serial.printf("[MQTT] SUBACK msg_id=%d granted=%u code=%d\n",e->msg_id,granted?1u:0u,
          e->data_len==1&&e->data?static_cast<unsigned char>(e->data[0]):255u);
      Lock lock;subscriptions.received(e->msg_id,granted);
    }
    else if(id==MQTT_EVENT_PUBLISHED&&e){
      Lock lock;self.pubacks_[self.pubackIndex_++%8]=e->msg_id;
    }
    else if(id==MQTT_EVENT_DATA&&e){
      Lock lock;
      if(!self.connected_.load())return;
      if(e->current_data_offset==0){
        fragmentTopic=-1;
        if(e->topic&&e->topic_len>0){
          if(size_t(e->topic_len)==strlen(commandTopic)&&!memcmp(e->topic,commandTopic,e->topic_len))fragmentTopic=0;
          else if(size_t(e->topic_len)==strlen(configTopic)&&!memcmp(e->topic,configTopic,e->topic_len))fragmentTopic=1;
        }
      }
      if(fragmentTopic>=0&&subscriptions.state(unsigned(fragmentTopic))==1){
        Ingress& target=fragmentTopic?configIngress:ingress;
        target.fragment(e->topic,e->topic_len,e->data,e->data_len,e->total_data_len,e->current_data_offset,e->retain,
            fragmentTopic?configTopic:commandTopic,commandIngressEpoch(),fragmentTopic==1);
      }
    }
  }
  esp_mqtt_client_handle_t handle_=nullptr;
  bool unhealthy_=false;
  uint64_t connectionGeneration_=0; // Protected by ingress mux, copied with envelopes.
  std::atomic<bool> connected_{false},failed_{false};
  std::atomic<bool> subscribeStarted_{false},subscribed_{false};
  int pubacks_[8]={-1,-1,-1,-1,-1,-1,-1,-1};
  unsigned pubackIndex_=0;
  uint32_t subscribeAt_=0,ackAt_=0;
  int ackFlight_=-1,ackSlot_=-1;
};
void worker(void*){
  // No secret/SDK error payload logging; some underlying logs include URIs.
  esp_log_level_set(kLogTag,ESP_LOG_NONE);
  uint8_t mac[6]={};
  const bool tlsReady=kTlsDateVerification;
  const bool macReady=device_identity::readStationMac(mac);
  const bool identityReady=macReady&&tlsReady&&topics.initialize(mac);
  const bool configReady=identityReady&&loadConfig(mac,config);
  const bool trustReady=configReady&&(kTlsNativeDates||prepareMqttTrust(config.ca));
  const bool ready=tlsReady&&identityReady&&configReady&&trustReady;
  Serial.printf("[MQTT] startup checks tls=%u identity=%u nvs_config=%u ca_ready=%u\n",
      tlsReady?1u:0u,identityReady?1u:0u,configReady?1u:0u,trustReady?1u:0u);
  if(!tlsReady)Serial.println("[MQTT] certificate date verification is unavailable in this build");
  else if(!macReady)Serial.println("[MQTT] could not read ESP32 station MAC");
  else if(!identityReady)Serial.println("[MQTT] could not initialize the device identity");
  else if(!configReady)Serial.println("[MQTT] MQTT config missing/invalid; check provisioned host, TLS port, CA, username and password");
  else if(!trustReady)Serial.println("[MQTT] CA certificate could not be prepared");
  logInternalHeap();
  uint32_t lastHeapLog=millis();
  EspClient client;Runtime runtime(millis);
  snprintf(commandTopic,sizeof(commandTopic),"iot/garden/%s/cmd",topics.identity);
  snprintf(configTopic,sizeof(configTopic),"iot/garden/%s/config",topics.identity);
  snprintf(ackTopic,sizeof(ackTopic),"iot/garden/%s/ack",topics.identity);
  for(;;){
    ::telemetry_snapshot::Snapshot copy;bool have;
    {Lock lock;copy=latest;have=observed;}
    const bool wifi=ble_wifi_service::networkSnapshot().online;
    const time_t utc=time(nullptr);
    bool clock=utc>=ble_commands::kUnixMin&&utc<=ble_commands::kUnixMax;
    static bool timeSyncStarted=false;
    static bool clockReadyLogged=false;
    if(wifi&&!clock&&!timeSyncStarted){
      configTime(0,0,"pool.ntp.org","time.google.com","time.cloudflare.com");
      timeSyncStarted=true;
      Serial.println("[MQTT] SNTP time sync requested");
    }
    if(!clock){
      const time_t synchronizedUtc=time(nullptr);
      clock=synchronizedUtc>=ble_commands::kUnixMin&&synchronizedUtc<=ble_commands::kUnixMax;
    }
    if(clock&&!clockReadyLogged){
      Serial.println("[MQTT] system clock ready");
      clockReadyLogged=true;
    }
    const State previousState=runtime.state();
    runtime.service(millis(),ready,wifi,clock,copy,have,client);
    if(runtime.state()==State::Backoff&&previousState!=State::Backoff)
      Serial.println("[MQTT] connection failed; retrying in 5 seconds");
    client.commands();
    const uint32_t now=millis();
    if(uint32_t(now-lastHeapLog)>=kHeapLogIntervalMs){
      logInternalHeap();
      lastHeapLog=now;
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
}
bool begin(){
  if(!kTlsDateVerification)return false;
  if(started)return false;
  started=true;
  return xTaskCreate(worker,"mqtt-telemetry",kWorkerStackBytes,nullptr,1,nullptr)==pdPASS;
}
bool connected(){return brokerConnected.load();}
void publish(const ::telemetry_snapshot::Snapshot& snapshot){Lock lock;latest=snapshot;observed=true;}
}
#else
namespace plant_mqtt {
bool connected(){return false;}
}
#endif
