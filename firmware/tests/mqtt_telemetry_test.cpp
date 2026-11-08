#include "../esp32/mqtt_telemetry.h"
#include <cassert>
#include "telemetry_water_fixture.h"
#include <iostream>
#include <string>
using namespace plant_mqtt;
struct Fake:Client {
  unsigned starts=0,stops=0,statuses=0,samples=0;bool up=false,error=false,allow=true,startOk=true;
  bool start()override{++starts;return startOk;}
  void stop()override{++stops;up=error=false;}
  bool connected()const override{return up;}
  bool failed()const override{return error;}
  bool status()override{++statuses;return allow;}
  bool sample(const telemetry_snapshot::Snapshot&,bool,uint32_t)override{++samples;return allow;}
};
int main(){
  char active[kActiveCapacity];
  const std::string expectedActive = std::string("{\"status\":\"ACTIVE\",\"firmware_version\":\"") + kFirmwareVersion + "\"}";
  assert(activeStatus(active, sizeof(active)) == expectedActive.size());
  assert(active == expectedActive);
  assert(!activeStatus(active, expectedActive.size()) && !active[0]);
  assert(!activeStatus(nullptr, 0));
  water_test::verify();
  Topics t;uint8_t mac[]={0x38,0x3e,0x51,0x6f,0xc7,0xa8};assert(t.initialize(mac));
  assert(std::string(t.telemetry)=="iot/garden/38:3E:51:6F:C7:A8/telemetry");
  assert(std::string(t.status)=="iot/garden/38:3E:51:6F:C7:A8/status");
  assert(std::string(t.client)=="ws-383E516FC7A8");
  telemetry_snapshot::Snapshot snapshot;auto& r=snapshot.reading;r.sampledAtMs=100;r.soilValid=r.dhtValid=true;
  r.soilPercent=42.5f;r.temperatureC=31.8f;r.humidityPercent=68;
  r.distanceValid=true;r.distanceCm=5;
  char out[256];size_t size=telemetry(t,snapshot,true,100,out,sizeof(out));assert(size==strlen(out));
  assert(std::string(out)=="{\"deviceId\":\"38:3E:51:6F:C7:A8\",\"soilMoisture\":42.5,\"temperature\":31.8,\"humidity\":68.0,\"waterLevel\":null}");
  assert(!strstr(out,"pump")&&!strstr(out,"recordedAt"));
  char tiny[8];assert(!telemetry(t,snapshot,true,100,tiny,sizeof(tiny))&&!tiny[0]);
  assert(!telemetry(t,snapshot,true,100,nullptr,0));
  for(bool observed:{false,true}){
    telemetry(t,snapshot,observed,4101,out,sizeof(out));assert(strstr(out,"\"soilMoisture\":null"));
    assert(strstr(out,"\"temperature\":null")&&strstr(out,"\"humidity\":null"));
  }
  r.sampledAtMs=UINT32_MAX-10;r.soilPercent=NAN;r.temperatureC=INFINITY;r.humidityPercent=101;
  telemetry(t,snapshot,true,5,out,sizeof(out));assert(!strstr(out,"nan")&&!strstr(out,"inf"));
  assert(strstr(out,"\"soilMoisture\":null")&&strstr(out,"\"temperature\":null"));
  Config config;assert(!configured(config));
  strcpy(config.host,"broker.example.invalid");strcpy(config.ca,"-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----");
  strcpy(config.username,"fixture-user");strcpy(config.password,"fixture-password");config.port=8883;
  assert(configured(config));strcpy(config.host,"ssl://broker.example.invalid");assert(!configured(config));
  strcpy(config.host,"broker.example.invalid");memset(config.password,'x',sizeof(config.password));assert(!configured(config));
  for(unsigned missing=0;missing<3;++missing){
    Runtime rt;Fake c;rt.service(0,missing!=0,missing!=1,missing!=2,snapshot,true,c);assert(!c.starts);
  }
  for(uint32_t origin:{0u,UINT32_MAX-1000}){
    Runtime rt;Fake c;uint32_t now=origin;
    auto tick=[&](uint32_t n){rt.service(n,true,true,true,snapshot,false,c);};
    tick(now);assert(c.starts==1&&rt.state()==State::Connecting);
    for(uint32_t delay:{5000u,10000u,20000u,30000u,30000u}){
      now+=15000;tick(now);assert(rt.state()==State::Backoff);
      unsigned before=c.starts;tick(now+delay-1);assert(c.starts==before);
      now+=delay;tick(now);assert(c.starts==before+1);
    }
    c.up=true;tick(++now);assert(c.statuses==1&&!c.samples&&rt.state()==State::Online);
    tick(now+9999);assert(c.statuses==1&&!c.samples);
    now+=10000;tick(now);assert(c.statuses==2&&c.samples==1); // no sensor still heartbeats/null sample
    now+=60000;tick(now);tick(now);assert(c.statuses==3&&c.samples==2); // no catch-up burst
    c.up=false;tick(++now);assert(rt.state()==State::Backoff);
    unsigned count=c.starts;tick(now+4999);assert(c.starts==count);now+=5000;tick(now);assert(c.starts==count+1);
    c.up=true;tick(++now);unsigned messages=c.statuses+c.samples;
    rt.service(++now,true,false,true,snapshot,true,c);assert(rt.state()==State::Offline&&!c.up);
    assert(c.statuses+c.samples==messages);
    tick(++now);c.up=true;tick(++now);c.allow=false;now+=10000;tick(now);assert(rt.state()==State::Backoff);
  }
  Runtime fault;Fake c;c.startOk=false;
  for(unsigned i=0;i<100;++i)fault.service(i*30000,true,true,true,snapshot,true,c);
  assert(c.starts==1&&fault.state()==State::Unavailable);
  std::cout<<"PASS MQTT DTO/null/stale/identity/bounds/timestamp omission, readiness, cadence/no burst, reconnect/rollover/failure policy (model only)\n";
}
