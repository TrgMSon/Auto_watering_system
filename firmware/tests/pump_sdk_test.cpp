// Execute the production ARDUINO branch with deterministic SDK/GPIO substitutes.
#include "pump_sdk/Arduino.h"
#include "pump_sdk/driver/gptimer.h"
#include "pump_sdk/esp_private/gptimer.h"
#include "pump_sdk/soc/gpio_struct.h"
#include "../esp32/autonomous_feature.h"
#include "../esp32/pump.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
struct FakeTimer {bool exists=false,registered=false,enabled=false,started=false;};
struct FakeInterrupt {bool shared=false,iram=false;};
static FakeTimer timer;
static FakeInterrupt interrupt;
gpio_dev_t GPIO;
int sdkLockDepth=0;
static uint32_t now=0;
static std::string scenario;
static std::vector<std::string> calls;
static gptimer_alarm_cb_t callback=nullptr;
static unsigned allocated=0,onWrites=0,outputModes=0,internalCalls=0;
static void called(const char* s){assert(sdkLockDepth==0);calls.emplace_back(s);}
static bool is(const char* s){return scenario==s;}
int64_t esp_timer_get_time(){return int64_t(now)*1000;}
void fakePinWrite(int pin,int level){
 assert(pin==23);if(level){assert(timer.started);++onWrites;
#if WATERING_AUTONOMOUS_TEST
 assert(interrupt.iram&&!interrupt.shared);
#endif
 }
 GPIO.level=level;
}
void pinMode(int pin,int mode){assert(pin==23&&mode==OUTPUT&&GPIO.level==LOW);++outputModes;}
bool esp_ptr_in_iram(const void* p){return !is("code_external") &&
 !(is("time_external")&&p==reinterpret_cast<const void*>(esp_timer_get_time));}
bool esp_ptr_in_dram(const void*){return !is("data_external");}
bool esp_ptr_internal(const void* p){
 ++internalCalls;
 return !(is("timer_external")&&p==&timer) && !(is("interrupt_external")&&p==&interrupt);
}
esp_err_t gptimer_new_timer(const gptimer_config_t* c,gptimer_handle_t* out){
 called("new");assert(!timer.exists&&c->resolution_hz==1000000&&!c->flags.intr_shared);
 if(is("new_error"))return ESP_FAIL;
 if(is("new_null")){*out=nullptr;return ESP_OK;}
 timer.exists=true;++allocated;*out=&timer;return ESP_OK;
}
esp_err_t gptimer_register_event_callbacks(gptimer_handle_t t,const gptimer_event_callbacks_t* c,void* user){
 called("register");assert(t==&timer&&timer.exists&&!timer.enabled&&!user&&c->on_alarm);
 callback=c->on_alarm;timer.registered=true;interrupt.shared=is("shared");
 return is("register_error")||is("cleanup_delete")?ESP_FAIL:ESP_OK;
}
esp_err_t gptimer_get_intr_handle(gptimer_handle_t t,intr_handle_t* out){
 called("get");assert(t==&timer&&timer.registered&&!timer.enabled&&!timer.started);
 if(is("get_error"))return ESP_FAIL;
 *out=is("get_null")?nullptr:&interrupt;return ESP_OK;
}
esp_err_t esp_intr_set_in_iram(intr_handle_t h,bool enabled){
 called("iram");assert(h==&interrupt&&enabled&&!timer.enabled&&!timer.started);
 if(h->shared||is("iram_error"))return ESP_ERR_INVALID_ARG;
 h->iram=true;return ESP_OK;
}
esp_err_t gptimer_set_alarm_action(gptimer_handle_t,const gptimer_alarm_config_t* c){
 called("alarm");assert(c->alarm_count==10000&&c->reload_count==0&&c->flags.auto_reload_on_alarm);
 return is("alarm_error")?ESP_FAIL:ESP_OK;
}
esp_err_t gptimer_enable(gptimer_handle_t){
 called("enable");assert(!timer.enabled&&timer.registered);
#if WATERING_AUTONOMOUS_TEST
 assert(interrupt.iram);
#endif
 if(is("enable_error"))return ESP_FAIL;
 timer.enabled=true;return is("enable_partial")?ESP_FAIL:ESP_OK;
}
esp_err_t gptimer_start(gptimer_handle_t){
 called("start");assert(timer.enabled);
 if(is("start_error"))return ESP_FAIL;
 timer.started=true;
 return is("start_partial")||is("cleanup_stop")||is("cleanup_disable")?ESP_FAIL:ESP_OK;
}
esp_err_t gptimer_stop(gptimer_handle_t){
 called("stop");if(is("cleanup_stop"))return ESP_FAIL;
 if(!timer.enabled)return ESP_ERR_INVALID_STATE;
 timer.started=false;return ESP_OK;
}
esp_err_t gptimer_disable(gptimer_handle_t){
 called("disable");if(is("cleanup_disable"))return ESP_FAIL;
 if(!timer.enabled||timer.started)return ESP_ERR_INVALID_STATE;
 timer.enabled=false;return ESP_OK;
}
esp_err_t gptimer_del_timer(gptimer_handle_t t){
 called("delete");assert(t==&timer&&timer.exists);
 if(is("cleanup_delete"))return ESP_FAIL;
 assert(!timer.enabled&&!timer.started);timer.exists=false;--allocated;callback=nullptr;return ESP_OK;
}
static void fire(){assert(callback);assert(!callback(&timer,nullptr,nullptr));assert(sdkLockDepth==0);}
int main(int argc,char** argv){
 scenario=argc==2?argv[1]:"success";
 #if WATERING_AUTONOMOUS_TEST
 assert(strcmp(pump::guardInitializationStatus(),"UNKNOWN")==0);
 assert(calls.empty()&&allocated==0&&onWrites==0&&outputModes==0);
#endif
 pump::begin();assert(GPIO.level==LOW&&outputModes==1);
#if WATERING_AUTONOMOUS_TEST
 const auto diagnosticCalls=calls.size();const auto diagnosticLevel=GPIO.level;
 for(unsigned i=0;i<100;++i)
  assert(strcmp(pump::guardInitializationStatus(),is("success")?"READY":"FAILED")==0);
 assert(calls.size()==diagnosticCalls&&GPIO.level==diagnosticLevel&&onWrites==0);
#endif
 if(!is("success")){
  assert(pump::start(1)==pump::Result::GuardUnavailable&&onWrites==0);
  const bool quarantined=is("cleanup_stop")||is("cleanup_disable")||is("cleanup_delete");
  assert(allocated==(quarantined?1u:0u));
  if(callback) { fire(); }
  assert(GPIO.level==LOW);
  const auto count=calls.size();scenario="success";pump::begin();
  assert(calls.size()==count&&pump::start(1)==pump::Result::GuardUnavailable&&onWrites==0);
 }else{
  std::vector<std::string> expected={"new","register"};
#if WATERING_AUTONOMOUS_TEST
  expected.push_back("get");expected.push_back("iram");assert(internalCalls==2);
#else
  assert(internalCalls==0);
#endif
  expected.push_back("alarm");expected.push_back("enable");expected.push_back("start");assert(calls==expected);
  assert(allocated==1&&pump::start(5)==pump::Result::On);
  auto run=pump::run();
#if WATERING_AUTONOMOUS_TEST
  const auto reads=calls.size();const auto highs=onWrites;
  assert(strcmp(pump::guardInitializationStatus(),"READY")==0);
  assert(GPIO.level==HIGH&&calls.size()==reads&&onWrites==highs);
  assert(pump::run().generation==run.generation&&pump::run().startedMs==run.startedMs);
#endif
  assert(pump::start(5)==pump::Result::AlreadyOn&&pump::run().generation==run.generation);
  now=249;fire();assert(GPIO.level==HIGH);now=250;fire();assert(GPIO.level==LOW);
  assert(pump::takeStopEvent().cause==pump::StopCause::SafetyLeaseExpired);
  assert(pump::start(5)==pump::Result::On);run=pump::run();
  for(unsigned i=100;i<5000;i+=100){now=run.startedMs+i;assert(pump::renewSafetyLease(now,4000));fire();}
  now=run.startedMs+5000;fire();assert(GPIO.level==LOW);
  assert(pump::takeStopEvent().cause==pump::StopCause::Timeout);
  const auto count=calls.size();pump::begin();assert(calls.size()==count&&allocated==1&&GPIO.level==LOW);
  assert(pump::start(6)==pump::Result::QualificationLimit); // Ceiling unchanged.
 }
 std::cout<<"PASS SDK init flag="<<WATERING_AUTONOMOUS_TEST<<" scenario="<<(argc==2?argv[1]:"success")<<"\n";
}
