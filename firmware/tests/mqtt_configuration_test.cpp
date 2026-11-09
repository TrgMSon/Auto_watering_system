#include "pump/mode_fixture.h"
#include "pump/Arduino.h"
#include "../esp32/mqtt_ingress.h"
#include "../esp32/ble_command_bridge.h"
#include "mqtt_backend_fixture.h"
#include <iostream>
#include <string>
#include <vector>
uint32_t fakeNow=0;
int outputLevel=-1,writes=0,outputModes=0;
using namespace plant_mqtt;
const char* identity="02:03:04:05:06:07";
struct Storage:configuration::Storage {
  std::vector<uint8_t> data;unsigned saves=0;bool fail=false,readFail=false,corrupt=false;
  bool read(uint8_t* bytes,size_t capacity,size_t& n)override{
    n=0;if(readFail||data.empty()||data.size()>capacity)return false;
    n=data.size();memcpy(bytes,data.data(),n);return true;
  }
  bool write(const uint8_t* bytes,size_t n)override{
    ++saves;if(fail)return false;
    data.assign(bytes,bytes+n);if(corrupt)data[10]^=1;return true;
  }
};
sensors::Reading sample(float soil=80){
  sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.distanceValid=true;
  r.soilPercent=soil;r.distanceCm=5;return r;
}
std::string wire(const std::string& id,const std::string& minimum="30.0",const std::string& maximum="70.0",
    const std::string& water="20",const std::string& minutes="2",bool automatic=false){
  return "{\"command_id\":\""+id+"\",\"device_id\":\""+identity+"\",\"timestamp\":\"2026-11-07T08:00:00Z\",\"config\":{"
    "\"auto_mode\":"+(automatic?std::string("true"):std::string("false"))+",\"soil_moisture_min\":"+minimum+
    ",\"soil_moisture_max\":"+maximum+",\"max_watering_duration_minutes\":"+minutes+
    ",\"air_humidity_min\":50.0,\"air_humidity_max\":80.0,\"air_temperature_min\":20.0,\"air_temperature_max\":35.0,\"min_water_level_alert\":"+water+"}}";
}
PumpCommand parsed(const std::string& text){PumpCommand c;assert(parseConfiguration(text.data(),text.size(),identity,c));return c;}
void backendCompatibility(){
  auto c=parsed(kBackendConfig);assert(c.valuesValid&&!c.hasDuration);
  Storage storage;test_mode::FreshSystem system;system.begin(&storage);system.update(sample());
  auto local=system.configurationStatus().active();local.maxRunSeconds=197;
  assert(system.updateConfiguration(local)==configuration::Result::Saved);
  assert(!strcmp(applyConfiguration(system,c),"OK"));
  assert(system.configurationStatus().active().maxRunSeconds==197);
  // Omission is resolved again against the current owner setting, never a parse default.
  system.stopManual();local=system.configurationStatus().active();local.maxRunSeconds=241;
  assert(system.updateConfiguration(local)==configuration::Result::Saved);
  assert(!strcmp(applyConfiguration(system,c),"OK"));
  assert(system.configurationStatus().active().maxRunSeconds==241);
  Commands cache;cache.connection(true);assert(cache.submit(c,0));
  auto supplied=c;supplied.hasDuration=true;supplied.values[3]=2;
  assert(!cache.submit(supplied,0)); // Same ID, different presence/content.
  const std::string fixture=kBackendConfig;
  for(const std::string field:{"\"auto_mode\":true,","\"soil_moisture_min\":30.0,",
      "\"soil_moisture_max\":70.0,","\"air_humidity_min\":50.0,","\"air_humidity_max\":80.0,",
      "\"air_temperature_min\":20.0,","\"air_temperature_max\":35.0,",",\"min_water_level_alert\":20"}){
    auto missing=fixture;missing.erase(missing.find(field),field.size());PumpCommand out;
    assert(!parseConfiguration(missing.data(),missing.size(),identity,out));
  }
  for(const std::string value:{"null","\"2\"","true","NaN"}){
    auto bad=fixture;bad.insert(bad.find("\"auto_mode\""),"\"max_watering_duration_minutes\":"+value+",");
    PumpCommand out;assert(!parseConfiguration(bad.data(),bad.size(),identity,out));
  }
  for(const std::string value:{"0","11","1.5","1e999"}){
    auto bad=fixture;bad.insert(bad.find("\"auto_mode\""),"\"max_watering_duration_minutes\":"+value+",");
    assert(!parsed(bad).valuesValid);
  }
  auto bad=fixture;bad.replace(bad.find("30.0"),4,"101.0");
  assert(!parsed(bad).valuesValid);
  const auto before=system.configurationStatus().active();
  assert(!strcmp(applyConfiguration(system,parsed(bad)),"INVALID_CONFIG"));
  assert(configuration::equal(before,system.configurationStatus().active()));
  Ingress ingress;const char* topic="iot/garden/02:03:04:05:06:07/config";
  char bytes[kCommandBytes];size_t n;
  assert(ingress.fragment(topic,int(strlen(topic)),fixture.data(),int(fixture.size()),int(fixture.size()),0,true,topic,0,true));
  assert(ingress.pop(bytes,n));c=parsed(std::string(bytes,n));
  system.stopManual();system.begin(&storage);system.update(sample(20));
  assert(system.startManual(1)==irrigation::Result::Accepted);
  assert(!strcmp(applyConfiguration(system,c),"PUMP_RUNNING"));
  assert(system.status().pumpOn&&system.configurationStatus().active().maxRunSeconds==241);
  system.stopManual();
  const char* pumpTopic="iot/garden/02:03:04:05:06:07/cmd";
  assert(!ingress.fragment(pumpTopic,int(strlen(pumpTopic)),"{}",2,2,0,true,pumpTopic));
}
void parser(){
  const auto text=wire("A");auto c=parsed(text);assert(c.isConfig&&c.valuesValid&&!c.automatic);
  assert(c.values[0]==30&&c.values[1]==70&&c.values[2]==20&&c.values[3]==2);
  for(size_t i=0;i<text.size();++i){PumpCommand out;assert(!parseConfiguration(text.data(),i,identity,out));}
  for(const auto& b:{text+"null",wire(""),wire("A","\"30\""),wire("A","null"),wire("A","NaN"),wire("A","01"),wire("A","1.")}){
    PumpCommand out;assert(!parseConfiguration(b.data(),b.size(),identity,out));
  }
  PumpCommand out;assert(!parseConfiguration(text.data(),text.size(),"wrong",out));
  auto duplicate=text;duplicate.insert(1,"\"command_id\":\"other\",");assert(!parseConfiguration(duplicate.data(),duplicate.size(),identity,out));
  duplicate=text;duplicate.insert(duplicate.find("\"auto_mode\""),"\"auto_mode\":true,");assert(!parseConfiguration(duplicate.data(),duplicate.size(),identity,out));
  auto unknown=text;unknown.insert(1,"\"action\":\"CONFIG\",");assert(!parseConfiguration(unknown.data(),unknown.size(),identity,out));
  for(const auto& b:{wire("bad","30","70","0"),wire("bad","30","30"),wire("bad","30","70","20","11"),
      wire("bad","-1"),wire("bad","30","101"),wire("bad","30","70","101"),wire("bad","30","70","20","1.5"),
      wire("bad","1e999"),wire("bad","30","70","20","4294967297")})assert(!parsed(b).valuesValid);
  assert(parsed(wire("exponent","3e1","7e1")).valuesValid);
}
void ownerStorage(){
  Storage storage;test_mode::FreshSystem system;system.begin(&storage);system.update(sample());
  // Seed non-default local calibration; MQTT must preserve both endpoints.
  auto local=system.configurationStatus().active();local.fullCm=2.5f;local.emptyCm=12;
  assert(system.updateConfiguration(local)==configuration::Result::Saved);
  auto c=parsed(wire("valid","35","75","25","3",true));
  assert(!strcmp(applyConfiguration(system,c),"OK"));
  assert(system.status().mode==irrigation::Mode::Auto&&!system.status().pumpOn);
  const auto active=system.configurationStatus().active();
  assert(active.soilStart==35&&active.soilStop==75&&active.waterMin==25&&active.maxRunSeconds==180);
  assert(active.fullCm==2.5f&&active.emptyCm==12);
  unsigned saved=storage.saves;
  // New ID with identical persisted config changes runtime mode without write.
  c.automatic=false;assert(!strcmp(applyConfiguration(system,c),"OK"));assert(storage.saves==saved);
  assert(system.status().mode==irrigation::Mode::Manual);
  for(const auto& bad:{wire("bad","30","70","0"),wire("bad","30","30"),wire("bad","30","70","20","11")}){
    assert(!strcmp(applyConfiguration(system,parsed(bad)),"INVALID_CONFIG"));
    assert(configuration::equal(active,system.configurationStatus().active())&&storage.saves==saved);
  }
  system.command("AUTO");
  c=parsed(wire("failure","36","76","25","4",false));
  storage.fail=true;assert(!strcmp(applyConfiguration(system,c),"STORAGE_ERROR"));storage.fail=false;
  assert(system.status().mode==irrigation::Mode::Manual&&configuration::equal(active,system.configurationStatus().active()));
  storage.readFail=true;assert(!strcmp(applyConfiguration(system,c),"STORAGE_ERROR"));storage.readFail=false;
  assert(system.status().mode==irrigation::Mode::Manual&&configuration::equal(active,system.configurationStatus().active()));
  // Write succeeded before readback failed: explicitly do NOT promise flash rollback.
  configuration::Config flash;assert(configuration::decode(storage.data.data(),storage.data.size(),flash)&&flash.soilStart==36);
  storage.corrupt=true;assert(!strcmp(applyConfiguration(system,c),"STORAGE_ERROR"));storage.corrupt=false;
  assert(system.status().mode==irrigation::Mode::Manual&&configuration::equal(active,system.configurationStatus().active()));
  assert(!strcmp(applyConfiguration(system,c),"OK"));
  // This regression fixture boots with MANUAL mode flash; AUTO reboot is tested in desired_mode_test.
  c.automatic=true;assert(!strcmp(applyConfiguration(system,c),"OK"));
  system.begin(&storage);assert(system.status().mode==irrigation::Mode::Manual&&!system.status().pumpOn);
  system.update(sample(20));assert(system.startManual(1)==irrigation::Result::Accepted);
  const auto before=system.configurationStatus().active();saved=storage.saves;
  assert(!strcmp(applyConfiguration(system,parsed(wire("busy"))),"PUMP_RUNNING"));
  assert(system.status().pumpOn&&configuration::equal(before,system.configurationStatus().active())&&storage.saves==saved);
  system.stopManual();
  // OFF-created cooldown survives configuration/mode changes.
  const auto state=system.status().decision.state;
  assert(!strcmp(applyConfiguration(system,parsed(wire("cooldown","32","72","25","2",true))),"OK"));
  assert(system.status().decision.state==state&&!system.status().pumpOn);
  // Fresh boot AUTO with safe dry readings may start, still bounded to 5 seconds.
  system.begin(&storage);system.update(sample(20));
  assert(!strcmp(applyConfiguration(system,parsed(wire("start","32","72","25","2",true))),"OK"));
  assert(system.status().pumpOn&&pump::run().durationMs==5000);system.stopManual();
  assert(system.status().decision.state==watering::State::Locked);
  const auto attempts=system.status().decision.wateringAttempts;
  assert(!strcmp(applyConfiguration(system,parsed(wire("keep-lock","33","73","25","2",true))),"OK"));
  assert(!system.status().pumpOn&&system.status().decision.state==watering::State::Locked&&
      system.status().decision.wateringAttempts==attempts);
}
void cacheAndAck(){
  Commands cache;cache.connection(true);char bytes[320];int ack=-1;Storage storage;test_mode::FreshSystem system;system.begin(&storage);
  auto a=parsed(wire("A")),b=parsed(wire("B","40","80"));
  assert(cache.submit(a,0));int slot=cache.claim(0);assert(slot>=0);
  cache.completeConfiguration(slot,applyConfiguration(system,a));
  assert(cache.ack(0,ack,bytes,sizeof(bytes),identity));
  assert(std::string(bytes)=="{\"device_id\":\"02:03:04:05:06:07\",\"command_id\":\"A\",\"status\":\"SUCCESS\",\"action\":\"CONFIG\",\"message\":\"OK\"}");
  cache.attempted(ack,0,true);cache.accepted(ack,0);
  assert(cache.submit(b,1));slot=cache.claim(1);cache.completeConfiguration(slot,applyConfiguration(system,b));
  const unsigned saves=storage.saves;assert(cache.submit(a,2)&&cache.claim(2)<0&&storage.saves==saves);
  a.values[0]=31;assert(!cache.submit(a,3)); // Never return old success for conflicting content.
  a=parsed(wire("A"));a.values[4]=51;assert(!cache.submit(a,3)); // Even ignored air_* counts as content.
  auto invalid=parsed(wire("invalid","30","30"));assert(cache.submit(invalid,3));slot=cache.claim(3);
  cache.completeConfiguration(slot,applyConfiguration(system,invalid));
  // Drain B ACK then validate the failure action/message.
  assert(cache.ack(3,ack,bytes,sizeof(bytes),identity));cache.accepted(ack,3);
  assert(cache.ack(3,ack,bytes,sizeof(bytes),identity));assert(strstr(bytes,"\"action\":\"CONFIG\"")&&strstr(bytes,"INVALID_CONFIG")&&strstr(bytes,"FAILED"));
  cache.accepted(ack,3);cache.reclaim(kDedupRetentionMs+4);assert(!cache.contains("A"));
  // Use the real owner busy/storage outcomes to verify failed CONFIG ACKs too.
  system.begin(&storage);system.update(sample(20));assert(system.startManual(1)==irrigation::Result::Accepted);
  for(const char* id:{"busy-ack","storage-ack"}){
    if(!strcmp(id,"storage-ack")){system.stopManual();storage.fail=true;}
    auto request=parsed(wire(id,"31","71"));assert(cache.submit(request,10));slot=cache.claim(10);
    cache.completeConfiguration(slot,applyConfiguration(system,request));
    assert(cache.ack(10,ack,bytes,sizeof(bytes),identity));
    assert(strstr(bytes,"\"status\":\"FAILED\"")&&strstr(bytes,"\"action\":\"CONFIG\""));
    assert(strstr(bytes,!strcmp(id,"busy-ack")?"PUMP_RUNNING":"STORAGE_ERROR"));cache.accepted(ack,10);
  }
  storage.fail=false;
  // Success and failed enqueue paths settle, then reclaim: no permanently full cache.
  for(unsigned i=0;i<48;++i){
    const uint32_t now=kDedupRetentionMs+100+i*5000;auto next=parsed(wire("seq"+std::to_string(i)));
    assert(cache.submit(next,now));slot=cache.claim(now);assert(slot>=0);cache.completeConfiguration(slot,"OK");
    if(i%2)cache.accepted(slot,now);else for(unsigned attempt=0;attempt<3;++attempt)cache.attempted(slot,now+2000*attempt,false);
  }
}
void bridge(){
  Storage storage;test_mode::FreshSystem system;system.begin(&storage);system.update(sample());
  commandConnection(true);auto config=parsed(wire("bridge", "35","75","25","2",true));
  auto epoch=commandIngressEpoch(),generation=commandConnectionGeneration();
  assert(queueCommandBatch(&config,&epoch,1,fakeNow,generation));ble_command_bridge::serviceOwner(system);
  assert(system.status().mode==irrigation::Mode::Auto);
  unsigned saved=storage.saves;
  char ackBytes[320];int ackSlot=-1;
  assert(commandAck(fakeNow,ackSlot,ackBytes,sizeof(ackBytes),identity));
  assert(strstr(ackBytes,"\"action\":\"CONFIG\"")&&strstr(ackBytes,"\"message\":\"OK\""));
  commandAckAttempt(ackSlot,fakeNow,true);ackAccepted(ackSlot,fakeNow);
  config=parsed(wire("disconnect"));epoch=commandIngressEpoch();generation=commandConnectionGeneration();
  commandConnection(false);commandConnection(true);
  assert(!queueCommandBatch(&config,&epoch,1,fakeNow,generation));
  assert(queueCommand(config,fakeNow));commandConnection(false);commandConnection(true);ble_command_bridge::serviceOwner(system);
  assert(storage.saves==saved&&system.status().mode==irrigation::Mode::Auto);
  config=parsed(wire("off-before-config","34","74","25","2",true));
  assert(queueCommand(config,fakeNow));
  auto ble=ble_command_bridge::connect(7);ble_commands::Command off;off.op=ble_commands::Op::Off;
  assert(ble_command_bridge::reserve(ble,1,off,ble_commands::Code::Ok,fakeNow));
  ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);
  assert(storage.saves==saved&&system.status().mode==irrigation::Mode::Manual&&!system.status().pumpOn);
  // Detached parsing captured before OFF cannot re-enable AUTO after OFF.
  config=parsed(wire("stale-config","33","73","25","2",true));
  assert(!queueCommand(config,fakeNow,epoch));
  PumpCommand mqttOff;mqttOff.on=false;strcpy(mqttOff.id,"mqtt-off");
  epoch=commandIngressEpoch();assert(queueCommand(config,fakeNow,epoch));assert(queueCommand(mqttOff,fakeNow));
  ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);
  assert(storage.saves==saved&&!system.status().pumpOn&&system.status().mode==irrigation::Mode::Manual);
}
void transport(){
  Subscriptions subs;subs.reset();subs.received(11,true);subs.requested(0,11);subs.requested(1,12);
  assert(subs.state(0)==1&&subs.state(1)==0);subs.received(99,true);assert(subs.state(1)==0);
  subs.received(12,false);assert(subs.state(1)==-1);subs.reset();assert(subs.state(0)==0);
  subs.received(12,true);subs.requested(1,12);assert(subs.state(1)==1&&subs.state(0)==0);
  Ingress configs,pumps;const char* topic="iot/garden/02:03:04:05:06:07/config";auto text=wire("fragment");
  char bytes[kCommandBytes];size_t n;
  for(size_t split=1;split<text.size();++split){
    configs.reset();assert(configs.fragment(topic,int(strlen(topic)),text.data(),int(split),int(text.size()),0,false,topic));
    assert(!configs.pop(bytes,n));assert(configs.fragment(nullptr,0,text.data()+split,int(text.size()-split),int(text.size()),int(split),false,topic));
    assert(configs.pop(bytes,n)&&parsed(std::string(bytes,n)).valuesValid);
  }
  assert(!configs.fragment(topic,int(strlen(topic)),text.data(),int(text.size()),int(text.size()),0,true,topic));
  assert(configs.fragment(topic,int(strlen(topic)),text.data(),int(text.size()),int(text.size()),0,true,topic,0,true));
  char retainedBytes[kCommandBytes];size_t retainedSize;
  assert(configs.pop(retainedBytes,retainedSize)&&parsed(std::string(retainedBytes,retainedSize)).valuesValid);
  assert(!configs.fragment(topic,int(strlen(topic)),text.data(),int(text.size()),int(text.size()),0,true,topic));
  for(unsigned i=0;i<4;++i)assert(configs.fragment(topic,int(strlen(topic)),text.data(),int(text.size()),int(text.size()),0,false,topic));
  assert(!configs.fragment(topic,int(strlen(topic)),text.data(),int(text.size()),int(text.size()),0,false,topic));
  // Full config ingress does not consume even one slot of the independent pump FIFO.
  assert(pumps.fragment("/cmd",4,"{}",2,2,0,false,"/cmd"));configs.reset();assert(!configs.pop(bytes,n));
}
int main(){backendCompatibility();parser();ownerStorage();cacheAndAck();bridge();transport();std::cout<<"MQTT configuration PASS\n";}
