#include "ble_wifi_service.h"
#ifdef ARDUINO
#include "wifi_esp32_adapters.h"
#include "wifi_build_config.h"
#include <Arduino.h>
#include <esp_mac.h>
#include <atomic>
namespace ble_wifi_service {
namespace {
#if !WIFI_FIXED_ENABLED
ble_wifi::Handoff slot;
#endif
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
struct Lock{Lock(){portENTER_CRITICAL(&mux);}~Lock(){portEXIT_CRITICAL(&mux);}};
std::atomic<bool> ready{false};bool started=false;
wifi_runtime::Snapshot network;
const char* stateName(wifi_runtime::State state){
  switch(state){
    case wifi_runtime::State::NoCredentials:return "no-credentials";
    case wifi_runtime::State::Connecting:return "connecting";
    case wifi_runtime::State::Online:return "online";
    case wifi_runtime::State::Backoff:return "backoff";
    case wifi_runtime::State::Provisioning:return "provisioning";
    default:return "unavailable";
  }
}
const char* reasonName(wifi_runtime::Reason reason){
  switch(reason){
    case wifi_runtime::Reason::None:return "none";
    case wifi_runtime::Reason::Missing:return "missing";
    case wifi_runtime::Reason::Invalid:return "invalid";
    case wifi_runtime::Reason::Timeout:return "timeout";
    case wifi_runtime::Reason::LinkLost:return "link-lost";
    case wifi_runtime::Reason::RadioFault:return "radio-fault";
    default:return "worker-unavailable";
  }
}
#if !WIFI_FIXED_ENABLED
const char* persistenceName(ble_wifi::Persistence persistence){
  switch(persistence){
    case ble_wifi::Persistence::Verified:return "verified";
    case ble_wifi::Persistence::NotCommitted:return "not-committed";
    default:return "unconfirmed";
  }
}
#endif
void publish(const wifi_runtime::Runtime& runtime){
  const auto copy=runtime.snapshot();
  bool changed;
  {Lock lock;changed=copy.state!=network.state||copy.reason!=network.reason||copy.online!=network.online;network=copy;}
  // Worker context, outside the critical section; log transitions, not every poll.
  if(changed)Serial.printf("[WIFI] runtime state=%s reason=%s online=%u\n",
      stateName(copy.state),reasonName(copy.reason),unsigned(copy.online));
}
#if !WIFI_FIXED_ENABLED
uint64_t current(){Lock lock;return slot.current();}
void complete(uint64_t generation,ble_commands::Code code){Lock lock;slot.complete(generation,code,millis());}
#endif
void worker(void*){
  uint8_t mac[6];
  if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK){Serial.println("[WIFI] worker identity-failed");vTaskDelete(nullptr);return;}
  // Stable lifetime; all SDK/NVS methods are invoked ONLY in this worker.
  static ble_wifi::EspRadioSdk sdk;
  static ble_wifi::EspRadio radio(sdk);
  static ble_wifi::NvsCredentialBlob blob;
  ble_wifi::CredentialStorage storage(blob,mac);
  wifi_runtime::Runtime runtime(radio,storage);
#if WIFI_FIXED_ENABLED
  ready.store(true);
  static_assert(sizeof(WIFI_FIXED_SSID)>1 && sizeof(WIFI_FIXED_SSID)<=33,
      "Fixed Wi-Fi SSID must be 1..32 bytes");
  static_assert(sizeof(WIFI_FIXED_PASSWORD)>=9 && sizeof(WIFI_FIXED_PASSWORD)<=64,
      "Fixed Wi-Fi password must be 8..63 bytes");
  ble_commands::WifiRequest fixed;
  memcpy(fixed.ssid,WIFI_FIXED_SSID,sizeof(WIFI_FIXED_SSID));
  memcpy(fixed.password,WIFI_FIXED_PASSWORD,sizeof(WIFI_FIXED_PASSWORD));
  Serial.println("[WIFI] fixed credentials enabled; BLE Wi-Fi provisioning compiled out");
  runtime.bootFixed(millis(),fixed);
  fixed.clear();
  publish(runtime);
  for(;;){
    runtime.service(millis());
    publish(runtime);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
#else
  ble_wifi::Attempt attempt(millis);
  ble_commands::WifiAuthentication request;
  ble_commands::WifiSecurity admittedSecurity;
  bool active=false;uint64_t generation=0;uint32_t attemptStarted=0;
  ready.store(true);
  runtime.boot(millis());
  publish(runtime);
  for(;;){
    if(!active){
      bool taken;
      {Lock lock;taken=slot.take(millis(),request);}
      if(taken){
        attemptStarted=millis();
        Serial.println("[WIFI] provisioning admitted");
        runtime.preempt();publish(runtime);
        generation=request.generation;
        admittedSecurity.connect(generation);admittedSecurity.update(generation,true,true,false);
        active=attempt.reserve(generation,request,true,false,185,millis());
        request.request.clear();
        if(!active){Serial.println("[WIFI] provisioning reserve-failed");complete(generation,ble_commands::Code::WifiFail);runtime.provisioned(millis(),ble_wifi::Persistence::NotCommitted);}
      }
    }
    if(active){
      const uint64_t connected=current();
      attempt.service(millis(),connected,radio,storage,admittedSecurity);
      ble_commands::Code code;
      if(attempt.result(connected,millis(),code)){
        Serial.printf("[WIFI] provisioning result=%s elapsed_ms=%lu persistence=%s radio_fault=%u\n",
            ble_commands::name(code),static_cast<unsigned long>(millis()-attemptStarted),
            persistenceName(attempt.persistence()),unsigned(radio.faulted()));
        runtime.provisioned(millis(),attempt.persistence());
        complete(generation,code);attempt.release();active=false;
      }else if(attempt.state()==ble_wifi::Attempt::State::Idle){
        Serial.printf("[WIFI] provisioning cancelled-or-expired elapsed_ms=%lu\n",static_cast<unsigned long>(millis()-attemptStarted));
        runtime.provisioned(millis(),attempt.persistence());
        complete(generation,ble_commands::Code::WifiFail);active=false;
      }
    }
    if(!active)runtime.service(millis());
    publish(runtime);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
#endif
}
}
bool begin(){
  if(started)return false;
  started=true;
#if WIFI_FIXED_ENABLED
  return xTaskCreate(worker,"wifi-runtime",6144,nullptr,1,nullptr)==pdPASS;
#else
  return xTaskCreate(worker,"wifi-provision",6144,nullptr,1,nullptr)==pdPASS;
#endif
}
bool available(){return ready.load()&&!WIFI_FIXED_ENABLED;}
wifi_runtime::Snapshot networkSnapshot(){Lock lock;return network;}
void connection(uint64_t generation){
#if !WIFI_FIXED_ENABLED
  Lock lock;slot.connection(generation);
#else
  (void)generation;
#endif
}
bool reserve(const ble_commands::WifiAuthentication& auth,uint32_t now){
#if !WIFI_FIXED_ENABLED
  Lock lock;return ready.load()&&slot.reserve(auth,now);
#else
  (void)auth;(void)now;return false;
#endif
}
bool result(uint32_t now,uint64_t& generation,ble_commands::Code& code){
#if !WIFI_FIXED_ENABLED
  Lock lock;return slot.result(now,generation,code);
#else
  (void)now;(void)generation;(void)code;return false;
#endif
}
void release(uint64_t generation){
#if !WIFI_FIXED_ENABLED
  Lock lock;slot.release(generation);
#else
  (void)generation;
#endif
}
}
#endif
