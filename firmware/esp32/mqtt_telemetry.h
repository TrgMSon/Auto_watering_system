#pragma once
#include "ble_telemetry.h"
#include "ble_clock.h"
#include "firmware_version.h"
namespace plant_mqtt {
constexpr uint32_t kCadenceMs=10000;
constexpr char kOffline[]="{\"status\":\"OFFLINE\"}";
constexpr size_t kActiveCapacity = sizeof("{\"status\":\"ACTIVE\",\"firmware_version\":\"\"}") + sizeof(kFirmwareVersion) - 1;
inline size_t activeStatus(char* out, size_t capacity) {
  if (!out || !capacity) return 0;
  const int n = snprintf(out, capacity,
      "{\"status\":\"ACTIVE\",\"firmware_version\":\"%s\"}", kFirmwareVersion);
  if (n <= 0 || size_t(n) >= capacity) { out[0] = 0; return 0; }
  return size_t(n);
}
struct Config {
  char host[254]={},ca[4096]={},username[64]={},password[128]={};
  uint16_t port=0;
};
template<size_t N> inline bool text(const char (&s)[N]){return s[0]&&memchr(s,0,N);}
inline bool configured(const Config& c){
  if(!text(c.host)||!text(c.ca)||!text(c.username)||!text(c.password)||!c.port)return false;
  // Hostname only, never URI/userinfo/path. SDK verifies the certificate against it.
  for(const char* p=c.host;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||
      (*p>='0'&&*p<='9')||*p=='.'||*p=='-'))return false;
  return strstr(c.ca,"-----BEGIN CERTIFICATE-----")&&strstr(c.ca,"-----END CERTIFICATE-----");
}
struct Topics {
  char identity[18]={},client[32]={},telemetry[64]={},status[64]={};
  bool initialize(const uint8_t (&mac)[6]){
    char name[16];if(!ble_telemetry::identity(mac,identity,name))return false;
    snprintf(client,sizeof(client),"ws-%02X%02X%02X%02X%02X%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    snprintf(telemetry,sizeof(telemetry),"iot/garden/%s/telemetry",identity);
    snprintf(status,sizeof(status),"iot/garden/%s/status",identity);return true;
  }
};
inline size_t telemetry(const Topics& topics,const ::telemetry_snapshot::Snapshot& snapshot,bool observed,
    uint32_t now,char* out,size_t capacity){
  if(!out||!capacity)return 0;
  out[0]=0;if(!topics.identity[0])return 0;
  const auto& reading=snapshot.reading;
  const bool fresh=observed&&uint32_t(now-reading.sampledAtMs)<=ble_telemetry::kFreshnessMs;
  char soil[16],temp[16],hum[16],water[16];
  ble_telemetry::number(soil,reading.soilPercent,fresh&&reading.soilValid,0,100);
  ble_telemetry::number(temp,reading.temperatureC,fresh&&reading.dhtValid,0,50);
  ble_telemetry::number(hum,reading.humidityPercent,fresh&&reading.dhtValid,0,100);
  ble_telemetry::number(water,snapshot.waterPercent,fresh&&snapshot.waterValid&&reading.distanceValid,0,100);
  // Calibrated operational %, not absolute height/volume. recordedAt omitted: server
  // receipt time; its LocalDateTime DTO has no verified UTC offset contract.
  const int n=snprintf(out,capacity,
      "{\"deviceId\":\"%s\",\"soilMoisture\":%s,\"temperature\":%s,\"humidity\":%s,\"waterLevel\":%s}",
      topics.identity,soil,temp,hum,water);
  if(n<=0||size_t(n)>=capacity){out[0]=0;return 0;}return size_t(n);
}
// Small worker policy seam; no wire protocol implementation. Only the MQTT
// worker invokes this interface and the real ESP-MQTT client.
struct Client {
  virtual ~Client(){}
  virtual bool start()=0;
  virtual void stop()=0;
  virtual bool connected()const=0;
  virtual bool failed()const=0;
  virtual bool status()=0;
  virtual bool sample(const ::telemetry_snapshot::Snapshot&,bool,uint32_t)=0;
};
enum class State { Unavailable, Offline, Connecting, Online, Backoff };
class Runtime {
 public:
  explicit Runtime(uint32_t (*monotonic)()=nullptr):monotonic_(monotonic){}
  void service(uint32_t now,bool ready,bool wifi,bool clock,const ::telemetry_snapshot::Snapshot& r,bool observed,Client& client){
    if(fault_)return;
    if(!ready||!clock||!wifi){
      if(running_){client.stop();running_=false;}
      state_=(!ready||!clock)?State::Unavailable:State::Offline;return;
    }
    if(state_==State::Backoff&&uint32_t(now-since_)<wait_)return;
    if(!running_){
      if(!client.start()){fault_=true;state_=State::Unavailable;return;}
      running_=true;state_=State::Connecting;since_=stamp(now);return;
    }
    if(client.failed()||(!client.connected()&&state_==State::Online)||
        (state_==State::Connecting&&uint32_t(now-since_)>=15000)){retry(now,client);return;}
    if(!client.connected())return;
    if(state_!=State::Online){
      state_=State::Online;level_=0;lastStatus_=lastSample_=now;
      if(!client.status()){retry(now,client);return;}
      lastStatus_=lastSample_=stamp(now);
      return;
    }
    // Status has priority and does not depend on sensor validity/freshness.
    if(uint32_t(now-lastStatus_)>=kCadenceMs){
      if(!client.status()){retry(now,client);return;}
      lastStatus_=stamp(now);
    }
    if(uint32_t(now-lastSample_)>=kCadenceMs){
      if(!client.sample(r,observed,now)){retry(now,client);return;}
      lastSample_=stamp(now);
    }
  }
  State state()const{return state_;}
 private:
  uint32_t stamp(uint32_t fallback)const{return monotonic_?monotonic_():fallback;}
  void retry(uint32_t now,Client& client){
    client.stop();running_=false;const uint32_t delays[]={5000,10000,20000,30000};
    wait_=delays[level_];if(level_<3)++level_;since_=stamp(now);state_=State::Backoff;
  }
  State state_=State::Unavailable;bool running_=false,fault_=false;unsigned level_=0;
  uint32_t since_=0,wait_=0,lastStatus_=0,lastSample_=0;
  uint32_t (*monotonic_)()=nullptr;
};
}
