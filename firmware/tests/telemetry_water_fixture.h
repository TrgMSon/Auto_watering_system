#pragma once
#include "pump/Arduino.h"
#include "../esp32/irrigation.h"
#include "../esp32/mqtt_telemetry.h"
#include <string>
#include <vector>
#include <limits>
// Each telemetry suite is a separate executable with one test translation unit.
uint32_t fakeNow=100;
int outputLevel=-1,writes=0,outputModes=0;
namespace water_test {
struct Store:configuration::Storage,desired_mode::Storage {
  std::vector<uint8_t> bytes;unsigned saves=0;
  Store(){
    configuration::Config config;config.fullCm=2;config.emptyCm=12;
    bytes.resize(configuration::kRecordSize);configuration::encode(config,bytes.data());
  }
  bool read(uint8_t* out,size_t cap,size_t& n)override{
    n=bytes.size();if(n>cap)return false;memcpy(out,bytes.data(),n);return true;
  }
  bool write(const uint8_t* in,size_t n)override{++saves;bytes.assign(in,in+n);return true;}
  bool readMode(uint8_t& value)override{value=0;return true;}
  bool writeMode(uint8_t value)override{return value==0;}
};
inline sensors::Reading reading(float distance){
  sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.dhtValid=r.distanceValid=true;
  r.soilPercent=42.5f;r.temperatureC=31.8f;r.humidityPercent=68;r.distanceCm=distance;return r;
}
inline std::string field(const char* json,const char* key){
  const std::string text=json,needle=std::string("\"")+key+"\":";
  const auto offset=text.find(needle);assert(offset!=std::string::npos);
  const auto start=offset+needle.size();return text.substr(start,text.find_first_of(",}",start)-start);
}
inline void equalWater(const telemetry_snapshot::Snapshot& snapshot,uint32_t now,const char* expected){
  const auto ble=ble_telemetry::serialize(snapshot,now);
  plant_mqtt::Topics topics;uint8_t mac[]={2,3,4,5,6,7};assert(topics.initialize(mac));
  char mqtt[256];assert(plant_mqtt::telemetry(topics,snapshot,true,now,mqtt,sizeof(mqtt)));
  assert(ble.size&&strlen(ble.bytes)==ble.size&&ble.size<ble_telemetry::kCapacity);
  assert(field(ble.bytes,"water")==expected&&field(mqtt,"waterLevel")==expected);
  assert(field(ble.bytes,"pump")== (snapshot.actuatorOn?"1":"0"));
  assert(!strstr(mqtt,"pump")&&!strstr(mqtt,"recordedAt"));
}
inline void verify(){
  fakeNow=100;Store storage;irrigation::System system;system.begin(&storage,&storage);
  const float distances[]={2,12,7,1,13,11,std::numeric_limits<float>::max()};
  const char* results[]={"100.0","0.0","50.0","100.0","0.0","10.0","0.0"};
  for(unsigned i=0;i<7;++i){
    system.update(reading(distances[i]));const auto snapshot=system.telemetrySnapshot(fakeNow);
    assert(snapshot.waterValid&&snapshot.reading.sampledAtMs==fakeNow);equalWater(snapshot,fakeNow,results[i]);
    if(i==5)assert(system.status().decision.reason==watering::Reason::LowWater);
  }
  for(float invalid:{0.0f,-1.0f,NAN,INFINITY}){
    system.update(reading(invalid));equalWater(system.telemetrySnapshot(fakeNow),fakeNow,"null");
  }
  auto invalid=reading(7);invalid.distanceValid=false;system.update(invalid);
  equalWater(system.telemetrySnapshot(fakeNow),fakeNow,"null");
  // Independent failures must not invalidate a valid water measurement.
  invalid=reading(7);invalid.soilValid=false;invalid.dhtValid=false;system.update(invalid);
  auto independent=system.telemetrySnapshot(fakeNow);equalWater(independent,fakeNow,"50.0");
  assert(field(ble_telemetry::serialize(independent,fakeNow).bytes,"soil")=="null");
  for(unsigned fault=0;fault<4;++fault){
    auto config=irrigation::confirmedConfig();
    if(fault==0)config.calibrationConfirmed=false;
    if(fault==1)config.emptyDistanceCm=config.fullDistanceCm;
    if(fault==2)config.fullDistanceCm=NAN;
    if(fault==3)config.fullDistanceCm=-1;
    irrigation::System bad(config);bad.begin();bad.update(reading(7));
    equalWater(bad.telemetrySnapshot(fakeNow),fakeNow,"null");
  }
  system.begin(&storage,&storage);system.update(reading(7));
  const auto old=system.telemetrySnapshot(fakeNow);equalWater(old,fakeNow,"50.0");
  auto changed=system.configurationStatus().active();changed.fullCm=4;
  assert(system.updateConfiguration(changed)==configuration::Result::Saved);
  auto current=system.telemetrySnapshot(fakeNow);equalWater(current,fakeNow,"62.5");
  equalWater(old,fakeNow,"50.0"); // Detached copy retains its old coherent calibration result.
  assert(old.reading.distanceCm==current.reading.distanceCm&&old.reading.sampledAtMs==current.reading.sampledAtMs);
  for(uint32_t start:{100u,UINT32_MAX-1000}){
    fakeNow=start;system.update(reading(7));auto mailbox=system.telemetrySnapshot(fakeNow);
    equalWater(mailbox,start+4000,"62.5");mailbox.publishedAtMs=start+4001;
    equalWater(mailbox,start+4001,"null");
    assert(field(ble_telemetry::serve(mailbox,start+4001).bytes,"water")=="null");
    fakeNow=start+4001;const auto republished=system.telemetrySnapshot(fakeNow);
    assert(republished.reading.sampledAtMs==start);equalWater(republished,fakeNow,"null");
    system.update(reading(8));equalWater(system.telemetrySnapshot(fakeNow),fakeNow,"50.0");
  }
  fakeNow=100;system.begin(&storage,&storage);system.update(reading(8));
  assert(system.startManual(1)==irrigation::Result::Accepted);
  auto on=system.telemetrySnapshot(fakeNow);assert(on.actuatorOn);equalWater(on,fakeNow,"50.0");system.stopManual();
  for(float value:{NAN,INFINITY,-INFINITY}){on.waterPercent=value;equalWater(on,fakeNow,"null");}
  on.waterValid=false;on.waterPercent=50;equalWater(on,fakeNow,"null");
  on.waterValid=true;on.waterPercent=100;on.reading.soilPercent=100;on.reading.temperatureC=50;on.reading.humidityPercent=100;
  equalWater(on,fakeNow,"100.0");const auto worst=ble_telemetry::serialize(on,fakeNow);
  assert(std::string(worst.bytes)=="{\"soil\":100.0,\"temp\":50.0,\"hum\":100.0,\"water\":100.0,\"pump\":1}");
  assert(worst.size<=96&&strlen(worst.bytes)==worst.size);
  plant_mqtt::Topics topics;uint8_t mac[]={2,3,4,5,6,7};topics.initialize(mac);
  char out[256];plant_mqtt::telemetry(topics,on,false,fakeNow,out,sizeof(out));assert(field(out,"waterLevel")=="null");
}
}
