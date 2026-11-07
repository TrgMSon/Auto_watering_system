#include "pump/Arduino.h"
#include "../esp32/mqtt_ingress.h"
#include "../esp32/ble_command_bridge.h"
#include <string>
#include <iostream>
uint32_t fakeNow=0;
int outputLevel=-1,writes=0,outputModes=0;
using namespace plant_mqtt;
static const char* identity="38:3E:51:6F:C7:A8";
static const char* topic="iot/garden/38:3E:51:6F:C7:A8/cmd";
std::string wire(const std::string& id,const std::string& params="\"state\":\"ON\",\"duration_seconds\":1"){
  return "{\"command_id\":\""+id+"\",\"device_id\":\""+identity+"\",\"timestamp\":\"2026-11-07T10:00:00Z\",\"action\":\"PUMP_CONTROL\",\"params\":{"+params+"}}";
}
PumpCommand command(const std::string& id,bool on=true,uint32_t seconds=1){
  PumpCommand c;strcpy(c.id,id.c_str());c.on=on;c.seconds=on?seconds:0;return c;
}
sensors::Reading sample(){sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.distanceValid=true;r.soilPercent=20;r.distanceCm=5;return r;}
void parser(){
  PumpCommand c;const auto literal=wire("CMD-921659f5-0000-4000-8000-0123456789ab");
  assert(parsePump(literal.data(),literal.size(),identity,c)&&c.on&&c.seconds==1);
  for(const char* p:{"\"state\":\"ON\"","\"state\":\"ON\",\"duration_seconds\":0","\"state\":\"ON\",\"duration_seconds\":1.0","\"state\":\"ON\",\"duration_seconds\":1e0","\"state\":\"ON\",\"duration_seconds\":4294967296","\"state\":\"ON\",\"duration_seconds\":-1","\"state\":\"ON\",\"duration_seconds\":\"1\"","\"state\":\"ON\",\"duration_seconds\":true","\"state\":\"ON\",\"duration_seconds\":01","\"state\":\"ON\",\"state\":\"OFF\",\"duration_seconds\":1","\"state\":\"AUTO\"","\"state\":null"}){
    auto b=wire("x",p);assert(!parsePump(b.data(),b.size(),identity,c));
  }
  for(const char* p:{"\"state\":\"OFF\"","\"state\":\"OFF\",\"duration_seconds\":9"}){
    auto b=wire("off",p);assert(parsePump(b.data(),b.size(),identity,c)&&!c.on&&!c.seconds);
  }
  auto b=wire(std::string(65,'x'));assert(!parsePump(b.data(),b.size(),identity,c));
  b=wire("x");assert(!parsePump(b.data(),b.size(),"wrong",c));
  b.insert(1,"\"command_id\":\"other\",");assert(!parsePump(b.data(),b.size(),identity,c));
  for(size_t i=0;i<literal.size();++i)assert(!parsePump(literal.data(),i,identity,c));
  b=literal+"null";assert(!parsePump(b.data(),b.size(),identity,c));
  b=wire("x\\\"y");assert(!parsePump(b.data(),b.size(),identity,c));
}
void fragments(){
  Ingress in;auto b=wire("fragment");char out[512];size_t n=0;PumpCommand c;
  for(size_t split=1;split<b.size();++split){
    in.reset();assert(in.fragment(topic,int(strlen(topic)),b.data(),int(split),int(b.size()),0,false,topic));
    assert(!in.pop(out,n));assert(in.fragment(nullptr,0,b.data()+split,int(b.size()-split),int(b.size()),int(split),false,topic));
    assert(in.pop(out,n)&&n==b.size()&&parsePump(out,n,identity,c));
  }
  in.reset();assert(!in.fragment(topic,int(strlen(topic)),b.data(),1,513,0,false,topic));
  assert(!in.fragment("wrong",5,b.data(),int(b.size()),int(b.size()),0,false,topic));
  assert(!in.fragment(topic,int(strlen(topic)),b.data(),int(b.size()),int(b.size()),0,true,topic));
  assert(in.fragment(topic,int(strlen(topic)),b.data(),10,int(b.size()),0,false,topic));
  assert(!in.fragment(nullptr,0,b.data()+10,2,int(b.size()),9,false,topic));
  assert(!in.pop(out,n));
  for(int i=0;i<4;++i)assert(in.fragment(topic,int(strlen(topic)),b.data(),int(b.size()),int(b.size()),0,false,topic));
  assert(!in.fragment(topic,int(strlen(topic)),b.data(),int(b.size()),int(b.size()),0,false,topic));
  in.reset();assert(!in.pop(out,n));
}
void cache(){
  Commands q;q.connection(true);auto c=command("same");
  assert(q.submit(c,0)&&q.submit(c,0));int slot=q.claim(0);assert(slot>=0&&q.claim(0)<0);
  auto conflict=c;conflict.seconds=2;assert(!q.submit(conflict,0));
  q.connection(false);q.complete(slot,irrigation::Result::Accepted);
  char bytes[320];int ack=-1;assert(!q.ack(0,ack,bytes,sizeof(bytes),identity));
  q.connection(true);assert(q.ack(0,ack,bytes,sizeof(bytes),identity));
  assert(std::string(bytes)=="{\"device_id\":\"38:3E:51:6F:C7:A8\",\"command_id\":\"same\",\"status\":\"SUCCESS\",\"action\":\"ON\",\"message\":\"ACCEPTED\"}");
  assert(strstr(bytes,"\"status\":\"SUCCESS\"")&&strstr(bytes,"\"action\":\"ON\""));
  for(unsigned i=0;i<3;++i){assert(q.ack(i*2000,ack,bytes,sizeof(bytes),identity));q.attempted(ack,i*2000);}
  assert(!q.ack(9000,ack,bytes,sizeof(bytes),identity));assert(q.submit(c,9000)&&q.claim(9000)<0);
  Commands full;full.connection(true);
  for(unsigned i=0;i<15;++i)assert(full.submit(command("full"+std::to_string(i)),0));
  assert(!full.submit(command("overflow"),0));assert(full.submit(command("off",false),0));
  slot=full.claim(0);assert(slot==0&&!full.record(slot).command.on);
  full.complete(slot,irrigation::Result::Accepted);assert(full.claim(0)<0);
  assert(!full.submit(command("new",false),0));
  Commands cancel;cancel.connection(true);assert(cancel.submit(command("pending"),0));cancel.connection(false);cancel.connection(true);assert(cancel.claim(0)<0);
  Commands expired;expired.connection(true);assert(expired.submit(command("expired"),0));assert(expired.claim(1000)<0);
  Commands delivered;delivered.connection(true);assert(delivered.submit(command("delivered"),0));
  slot=delivered.claim(0);delivered.complete(slot,irrigation::Result::Accepted);
  delivered.attempted(slot,0);delivered.accepted(slot,0);assert(!delivered.ack(10000,ack,bytes,sizeof(bytes),identity));
}
void retention(){
  char bytes[320];int ack=-1;
  Commands full;full.connection(true);
  for(unsigned i=0;i<16;++i){
    auto c=command("retained"+std::to_string(i),i!=15);
    assert(full.submit(c,10));int slot=full.claim(10);assert(slot>=0);
    full.complete(slot,irrigation::Result::Accepted);full.attempted(slot,10,true);full.accepted(slot,10);
  }
  const uint32_t before=10+kDedupRetentionMs-1;
  assert(!full.submit(command("too-early"),before));
  for(unsigned i=0;i<16;++i){
    assert(full.submit(command("retained"+std::to_string(i),i!=15),before));
    assert(full.claim(before)<0); // No duplicate execution; duplicates do not refresh TTL.
  }
  assert(full.submit(command("retained0"),before+1)); // Expired ID becomes new work.
  assert(full.claim(before+1)>=0);
  assert(!full.contains("retained15"));

  Commands protectedRecords;protectedRecords.connection(true);
  for(unsigned i=0;i<15;++i)assert(protectedRecords.submit(command("protected"+std::to_string(i)),0));
  int admitted=protectedRecords.claim(0);assert(admitted>=0);
  protectedRecords.reclaim(10*kDedupRetentionMs);
  assert(protectedRecords.record(admitted).state==Commands::Record::Admitted);
  assert(protectedRecords.record(2).state==Commands::Record::Pending);
  protectedRecords.complete(admitted,irrigation::Result::Accepted);
  protectedRecords.reclaim(10*kDedupRetentionMs); // Done, but ACK not started.
  assert(protectedRecords.contains("protected0"));
  assert(!protectedRecords.submit(command("still-full"),10*kDedupRetentionMs));
  protectedRecords.attempted(admitted,0,false);
  protectedRecords.attempted(admitted,2000,false);
  protectedRecords.attempted(admitted,4000,true); // Last attempt is still awaiting PUBACK.
  protectedRecords.reclaim(10*kDedupRetentionMs);
  assert(protectedRecords.contains("protected0"));
  const uint32_t settled=10*kDedupRetentionMs;
  protectedRecords.finishAttempt(admitted,settled);
  protectedRecords.reclaim(settled+kDedupRetentionMs-1);assert(protectedRecords.contains("protected0"));
  protectedRecords.reclaim(settled+kDedupRetentionMs);assert(!protectedRecords.contains("protected0"));

  Commands rollover;rollover.connection(true);assert(rollover.submit(command("wrap"),0));
  int slot=rollover.claim(0);rollover.complete(slot,irrigation::Result::Accepted);
  const uint32_t end=UINT32_MAX-20;rollover.accepted(slot,end);
  rollover.reclaim(uint32_t(end+kDedupRetentionMs-1));assert(rollover.contains("wrap"));
  rollover.reclaim(uint32_t(end+kDedupRetentionMs));assert(!rollover.contains("wrap"));

  // More than sixteen sequential commands run through the real System owner API.
  Commands sequential;sequential.connection(true);irrigation::System system;system.begin();
  uint32_t now=0;unsigned executions=0;
  for(unsigned i=0;i<48;++i){
    auto c=command("sequential"+std::to_string(i),false);
    assert(sequential.submit(c,now));slot=sequential.claim(now);assert(slot>=0);
    const auto outcome=system.stopManual();assert(outcome==irrigation::Result::Accepted);++executions;
    sequential.complete(slot,outcome);sequential.attempted(slot,now,true);
    assert(!sequential.ack(now,ack,bytes,sizeof(bytes),identity));
    sequential.accepted(slot,now);
    assert(sequential.submit(c,now+3999));assert(sequential.claim(now+3999)<0);
    now+=4000; // Fifteen retained results coexist; expired slots support the next batch.
  }
  assert(executions==48);
}
void owner(){
  irrigation::System system;system.begin();system.update(sample());commandConnection(true);
  assert(queueCommand(command("on"),fakeNow));ble_command_bridge::serviceOwner(system);assert(system.status().pumpOn);
  commandConnection(false);assert(system.status().pumpOn);commandConnection(true);
  const auto run=pump::run();fakeNow+=10;system.update(sample());
  assert(queueCommand(command("repeat",true,5),fakeNow));ble_command_bridge::serviceOwner(system);
  assert(pump::run().startedMs==run.startedMs&&pump::run().durationMs==run.durationMs);
  assert(queueCommand(command("stop",false),fakeNow));ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  // BLE ON waiting -> MQTT OFF cancels it before either command is admitted.
  auto generation=ble_command_bridge::connect(4);ble_commands::Command on;on.op=ble_commands::Op::On;on.seconds=1;
  assert(ble_command_bridge::reserve(generation,1,on,ble_commands::Code::Ok,fakeNow));
  assert(queueCommand(command("mqtt-off",false),fakeNow));ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  ble_commands::Dispatch::Slot result;assert(ble_command_bridge::result(result)&&result.code==ble_commands::Code::None);ble_command_bridge::release(result.ticket);
  // MQTT ON waiting -> BLE OFF cancels it.
  const uint64_t staleEpoch=commandIngressEpoch();
  assert(queueCommand(command("cancel-on"),fakeNow));ble_commands::Command off;off.op=ble_commands::Op::Off;
  assert(ble_command_bridge::reserve(generation,2,off,ble_commands::Code::Ok,fakeNow));
  ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  assert(ble_command_bridge::result(result));ble_command_bridge::release(result.ticket);
  // Bytes received before OFF, parsed/authenticated only after OFF, cannot restart.
  assert(!queueCommand(command("raw-old-on"),fakeNow,staleEpoch));
  // OFF during detached parsing still invalidates the captured ON epoch.
  PumpCommand parsedOn=command("parse-off-gap");
  assert(queueCommandBatch(&parsedOn,&staleEpoch,1,fakeNow,commandConnectionGeneration()));
  ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  assert(!ble_command_bridge::reserve(generation,3,on,ble_commands::Code::Ok,fakeNow,staleEpoch));
  const uint64_t bleOld=commandIngressEpoch();
  assert(queueCommand(command("raw-barrier-off",false),fakeNow));ble_command_bridge::serviceOwner(system);
  assert(!ble_command_bridge::reserve(generation,4,on,ble_commands::Code::Ok,fakeNow,bleOld));
  // A detached batch belongs to its original MQTT connection, including OFF.
  const uint64_t capturedGeneration=commandConnectionGeneration();
  const uint64_t capturedEpoch=commandIngressEpoch();
  const auto detached=wire("parse-session-gap");
  commandConnection(false);commandConnection(true);
  assert(parsePump(detached.data(),detached.size(),identity,parsedOn));
  assert(!queueCommandBatch(&parsedOn,&capturedEpoch,1,fakeNow,capturedGeneration));
  PumpCommand parsedOff=command("parse-old-off",false);
  assert(!queueCommandBatch(&parsedOff,&capturedEpoch,1,fakeNow,capturedGeneration));
  ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  commandConnection(false);commandConnection(true);
  assert(queueCommand(command("disconnect-on"),fakeNow));commandConnection(false);commandConnection(true);ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  // Real owner rejection: qualification and sensor gates; OFF remains accepted.
  assert(queueCommand(command("six",true,6),fakeNow));ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  system.update(sensors::Reading{});assert(queueCommand(command("sensor"),fakeNow));ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  assert(queueCommand(command("sensor-off",false),fakeNow));ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
  PumpCommand batch[2]={command("batch-on"),command("batch-off",false)};
  uint64_t epochs[2]={commandIngressEpoch(),commandIngressEpoch()};
  queueCommandBatch(batch,epochs,2,fakeNow);ble_command_bridge::serviceOwner(system);ble_command_bridge::serviceOwner(system);assert(!system.status().pumpOn);
}
int main(){parser();fragments();cache();retention();owner();std::cout<<"MQTT commands PASS\n";}

