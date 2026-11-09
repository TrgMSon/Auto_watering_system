#include "ble_wifi_service.h"
#ifdef ARDUINO
#include "wifi_esp32_adapters.h"
#include <Arduino.h>
#include <esp_mac.h>
#include <atomic>
namespace ble_wifi_service {
namespace {
ble_wifi::Handoff slot;
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
struct Lock{Lock(){portENTER_CRITICAL(&mux);}~Lock(){portEXIT_CRITICAL(&mux);}};
std::atomic<bool> ready{false};bool started=false;
wifi_runtime::Snapshot network;
void publish(const wifi_runtime::Runtime& runtime){const auto copy=runtime.snapshot();Lock lock;network=copy;}
uint64_t current(){Lock lock;return slot.current();}
void complete(uint64_t generation,ble_commands::Code code){Lock lock;slot.complete(generation,code,millis());}
void worker(void*){
  uint8_t mac[6];
  if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK){vTaskDelete(nullptr);return;}
  // Stable lifetime; all SDK/NVS methods are invoked ONLY in this worker.
  static ble_wifi::EspRadioSdk sdk;
  static ble_wifi::EspRadio radio(sdk);
  static ble_wifi::NvsCredentialBlob blob;
  ble_wifi::CredentialStorage storage(blob,mac);
  wifi_runtime::Runtime runtime(radio,storage);
  ble_wifi::Attempt attempt(millis);
  ble_commands::WifiAuthentication request;
  ble_commands::WifiSecurity admittedSecurity;
  bool active=false;uint64_t generation=0;
  ready.store(true);
  runtime.boot(millis());publish(runtime);
  for(;;){
    if(!active){
      bool taken;
      {Lock lock;taken=slot.take(millis(),request);}
      if(taken){
        runtime.preempt();publish(runtime);
        generation=request.generation;
        admittedSecurity.connect(generation);admittedSecurity.update(generation,true,true,false);
        active=attempt.reserve(generation,request,true,false,185,millis());
        request.request.clear();
        if(!active){complete(generation,ble_commands::Code::WifiFail);runtime.provisioned(millis(),ble_wifi::Persistence::NotCommitted);}
      }
    }
    if(active){
      const uint64_t connected=current();
      attempt.service(millis(),connected,radio,storage,admittedSecurity);
      ble_commands::Code code;
      if(attempt.result(connected,millis(),code)){
        runtime.provisioned(millis(),attempt.persistence());
        complete(generation,code);attempt.release();active=false;
      }else if(attempt.state()==ble_wifi::Attempt::State::Idle){
        runtime.provisioned(millis(),attempt.persistence());
        complete(generation,ble_commands::Code::WifiFail);active=false;
      }
    }
    if(!active)runtime.service(millis());
    publish(runtime);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}
bool begin(){
  if(started)return false;
  started=true;
  return xTaskCreate(worker,"wifi-provision",6144,nullptr,1,nullptr)==pdPASS;
}
bool available(){return ready.load();}
wifi_runtime::Snapshot networkSnapshot(){Lock lock;return network;}
void connection(uint64_t generation){Lock lock;slot.connection(generation);}
bool reserve(const ble_commands::WifiAuthentication& auth,uint32_t now){Lock lock;return ready.load()&&slot.reserve(auth,now);}
bool result(uint32_t now,uint64_t& generation,ble_commands::Code& code){Lock lock;return slot.result(now,generation,code);}
void release(uint64_t generation){Lock lock;slot.release(generation);}
}
#endif
