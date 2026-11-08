#include "pump/Arduino.h"
#include "../esp32/irrigation.h"
#include "../esp32/ble_command_bridge.h"
#include "../esp32/mqtt_commands.h"
#include "../esp32/mqtt_ingress.h"
#include "mqtt_backend_fixture.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <iostream>
#include <vector>
uint32_t fakeNow=0;int outputLevel=-1,writes=0,outputModes=0;
using namespace autonomous_storage;
static uint64_t epoch=0;
static bool gateAllowed=true,offAtStart=false;
static uint64_t capture(){return epoch;}
static bool allowed(uint64_t e){return gateAllowed&&e==epoch&&ble_command_bridge::activationAllowed(e);}
static pump::Result startReserved(uint64_t e,uint32_t seconds){
 if(offAtStart){offAtStart=false;++epoch;return pump::Result::Off;}
 if(!allowed(e))return pump::Result::Off;
 return ble_command_bridge::startReserved(e,seconds);
}
struct Memory:Storage {
 std::vector<uint8_t> bytes;bool failWrite=false,failRead=false;unsigned commits=0;
 std::function<void()> during;
 Read read(uint8_t* p,size_t cap,size_t& n)override {
  assert(!pump::isOn());n=0;if(failRead)return Read::Error;if(bytes.empty())return Read::Missing;
  assert(cap>=bytes.size());n=bytes.size();memcpy(p,bytes.data(),n);return Read::Ok;
 }
 bool writeCommit(const uint8_t* p,size_t n)override {
  assert(!pump::isOn());++commits;bytes.assign(p,p+n);
  if(during){auto hook=during;during=nullptr;hook();}
  return !failWrite;
 }
};
struct Modes:desired_mode::Storage {
 uint8_t value=0;bool readMode(uint8_t& v)override{assert(!pump::isOn());v=value;return true;}
 bool writeMode(uint8_t v)override{assert(!pump::isOn());value=v;return true;}
};
struct ConfigStore:configuration::Storage {
 std::vector<uint8_t> bytes;
 ConfigStore(){configuration::Config c;bytes.resize(configuration::kRecordSize);uint8_t b[configuration::kRecordSize];configuration::encode(c,b);bytes.assign(b,b+sizeof(b));}
 bool read(uint8_t* p,size_t cap,size_t& n)override{assert(!pump::isOn());n=bytes.size();assert(cap>=n);memcpy(p,bytes.data(),n);return true;}
 bool write(const uint8_t* p,size_t n)override{assert(!pump::isOn());bytes.assign(p,p+n);return true;}
};
struct Fixture {
 Memory mem;Ledger ledger{mem};Modes modes;ConfigStore config;irrigation::System system;
 Fixture(bool initialize=true,bool cleanBoot=false){fakeNow=0;epoch=plant_mqtt::commandIngressEpoch();gateAllowed=true;offAtStart=false;pump::testGuardFailure(false,false);pump::testGuardEnable(false);
  // Previous fixture may end mid-run: only GPIO OFF, never operate hardware.
  pump::command("OFF");
  if(cleanBoot){Record r;r.sequence=1;r.flags=Cooldown;uint8_t b[kRecordSize];encode(r,b);mem.bytes.assign(b,b+sizeof(b));}
  else if(initialize)assert(ledger.maintenanceInitialize(true,0)==Result::Ok);
  system.setActivationGate(capture,allowed,startReserved);system.begin(&config,&modes,&ledger);
  system.update(sample());
 }
 sensors::Reading sample(float soil=20,float cm=4){sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.distanceValid=true;r.soilPercent=soil;r.distanceCm=cm;return r;}
 void update(uint32_t dt=0,float soil=20,float cm=4){
  while(dt>2000){fakeNow+=2000;dt-=2000;system.update(sample(soil,cm));}
  fakeNow+=dt;system.update(sample(soil,cm));
 }
 void autoStart(){assert(system.command("AUTO")==irrigation::Result::Accepted);update(30000);}
};
static unsigned cases=0;static void pass(){++cases;}
int main(){
 {Fixture f;plant_mqtt::commandConnection(true);
  plant_mqtt::Ingress ingress;const char* topic="iot/garden/02:03:04:05:06:07/config";
  assert(ingress.fragment(topic,int(strlen(topic)),kBackendConfig,int(strlen(kBackendConfig)),int(strlen(kBackendConfig)),0,true,topic,plant_mqtt::commandIngressEpoch(),true));
  char bytes[plant_mqtt::kCommandBytes];size_t n;uint64_t ingressEpoch;
  assert(ingress.pop(bytes,n,&ingressEpoch));plant_mqtt::PumpCommand config;
  assert(plant_mqtt::parseConfiguration(bytes,n,"02:03:04:05:06:07",config));
  assert(plant_mqtt::queueCommand(config,fakeNow,ingressEpoch));ble_command_bridge::serviceOwner(f.system);
  assert(!pump::isOn()&&f.ledger.record().total==0); // No bypass of boot safety evidence.
  f.update(30000);assert(pump::isOn()&&pump::run().durationMs==5000&&f.ledger.record().total==1);
  const auto before=f.ledger.record().total;
  assert(!strcmp(plant_mqtt::applyConfiguration(f.system,config),"PUMP_RUNNING"));
  assert(f.ledger.record().total==before&&pump::isOn());
  f.system.stopManual();
  assert(!strcmp(plant_mqtt::applyConfiguration(f.system,config),"OK"));
  assert(!pump::isOn()&&f.ledger.record().total==before); // Config does not clear cooldown/accounting.
  f.system.stopManual();plant_mqtt::commandConnection(false);pass();}
 {Fixture f;f.autoStart();assert(pump::isOn()&&f.ledger.record().total==1);f.update(5000);assert(!pump::isOn()&&!(f.ledger.record().flags&Pending));pass();}
 {Fixture f;f.update(30000);assert(f.system.startManual(2)==irrigation::Result::Accepted);auto generation=pump::run().generation;
  assert(f.system.startManual(5)==irrigation::Result::AlreadyOn);assert(pump::run().generation==generation&&f.ledger.record().total==1);
  f.system.command("OFF");assert(!pump::isOn()&&f.system.status().mode==irrigation::Mode::Manual&&f.ledger.record().total==1);pass();}
 for(bool readback:{false,true}){Fixture f;f.update(30000);if(readback)f.mem.during=[&]{f.mem.failRead=true;};else f.mem.failWrite=true;
  assert(f.system.startManual(1)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::StorageFault);pass();}
 {Fixture f;f.update(30000);f.mem.during=[&]{f.system.command("MANUAL");};assert(f.system.startManual(2)!=irrigation::Result::Accepted);
  assert(!pump::isOn()&&f.ledger.record().total==1&&!(f.ledger.record().flags&Pending));pass();}
 {Fixture f;f.update(30000);f.mem.during=[&]{++epoch;};assert(f.system.startManual(2)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.ledger.record().total==1);pass();}
 {Fixture f;f.update(30000);f.mem.during=[&]{auto r=f.sample();r.soilValid=false;f.system.update(r);};
  assert(f.system.startManual(2)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.ledger.record().total==1);pass();}
 {Fixture f;f.update(30000);pump::testGuardFailure(false,true);assert(f.system.startManual(2)!=irrigation::Result::Accepted);
  assert(!pump::isOn()&&(f.ledger.record().flags&Maintenance)&&f.system.status().autonomousBlock==watering::Block::ActuatorFault);pump::testGuardFailure(false,false);pass();}
 {Fixture f;f.update(30000);assert(f.system.startManual(1)==irrigation::Result::Accepted);f.update(999);assert(pump::isOn());f.update(1);assert(!pump::isOn());pass();}
 {Fixture f;f.update(30000);pump::testGuardEnable(true);assert(f.system.startManual(5)==irrigation::Result::Accepted);fakeNow+=250;pump::testGuardInterrupt();f.system.service();
  assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::ActuatorFault);pump::testGuardEnable(false);pass();}
 {Fixture f;f.update(30000);f.system.startManual(1);f.mem.failWrite=true;f.update(1000);
  assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::StorageFault);pass();}
 {Fixture f;f.autoStart();pump::command("OFF");Ledger reboot(f.mem);irrigation::System s;s.setActivationGate(capture,allowed,startReserved);s.begin(nullptr,&f.modes,&reboot);
  fakeNow+=30000;s.update(f.sample());assert(!pump::isOn()&&s.status().autonomousBlock==watering::Block::StorageFault);pass();}
 {Fixture f;f.autoStart();f.update(5000);Ledger reboot(f.mem);irrigation::System s;s.setActivationGate(capture,allowed,startReserved);s.begin(nullptr,&f.modes,&reboot);
  const uint32_t boot=fakeNow;
  for(unsigned i=0;i<15;++i){fakeNow=boot+i*2000;s.update(f.sample());}
  fakeNow=boot+29999;s.update(f.sample());assert(!pump::isOn());++fakeNow;s.update(f.sample());assert(pump::isOn()&&reboot.record().total==2);s.stopManual();pass();}
 {Fixture f;f.autoStart();f.update(5000);f.update(30000);f.update(5000);assert(f.ledger.record().total==2);
  Ledger reboot(f.mem);irrigation::System s;s.setActivationGate(capture,allowed,startReserved);s.begin(nullptr,&f.modes,&reboot);
  const uint32_t boot=fakeNow;
  for(unsigned i=0;i<=15;++i){fakeNow=boot+i*2000;s.update(f.sample());}
  fakeNow=boot+1799999;s.update(f.sample());assert(!pump::isOn());++fakeNow;s.update(f.sample());assert(pump::isOn()&&reboot.record().total==3);s.stopManual();pass();}
 {Fixture f;f.autoStart();f.update(5000);f.update(30000);f.update(5000);f.update(1800000);f.update(5000);f.update(30000);f.update(5000);f.update(30000);
  assert(!pump::isOn()&&f.ledger.record().total==4&&f.system.status().autonomousBlock==watering::Block::BudgetExhausted);
  f.update(2000,80);for(int i=0;i<15;++i)f.update(2000,80);assert(f.ledger.record().total==0&&!pump::isOn());pass();}
 {Fixture f;f.autoStart();f.system.stopManual();auto total=f.ledger.record().total;
  for(int i=0;i<3;++i){auto c=f.system.configurationStatus().active();c.soilStart=25;assert(f.system.applyRemoteConfiguration(c,false)==irrigation::RemoteConfigResult::Ok);}
  assert(f.ledger.record().total==total&&!pump::isOn());pass();}
 {Fixture f(false);f.system.command("AUTO");f.update(30000);assert(!pump::isOn()&&f.mem.commits==0);pass();}
 {Fixture f;f.update(30000);assert(f.system.startManual(6)==irrigation::Result::QualificationLimit&&f.ledger.record().total==0);pass();}

 {Fixture f;f.update(30000);offAtStart=true;assert(f.system.startManual(1)!=irrigation::Result::Accepted);
  assert(!pump::isOn()&&f.ledger.record().total==1&&!(f.ledger.record().flags&Pending));pass();}

 {Fixture f;f.update(30000);plant_mqtt::commandConnection(true);plant_mqtt::PumpCommand on;
  strcpy(on.id,"b31-ON");on.on=true;on.seconds=1;
  assert(plant_mqtt::queueCommand(on,fakeNow));ble_command_bridge::serviceOwner(f.system);
  assert(pump::isOn()&&f.ledger.record().total==1);
  assert(plant_mqtt::queueCommand(on,fakeNow));ble_command_bridge::serviceOwner(f.system);assert(f.ledger.record().total==1);
  plant_mqtt::PumpCommand off;strcpy(off.id,"b31-OFF");assert(plant_mqtt::queueCommand(off,fakeNow));ble_command_bridge::serviceOwner(f.system);
  assert(!pump::isOn()&&f.ledger.record().total==1);plant_mqtt::commandConnection(false);pass();}
 {Fixture f;f.update(30000);ble_commands::Command on;on.op=ble_commands::Op::On;on.seconds=2;
  assert(ble_commands::execute(f.system,on)==ble_commands::Code::Ok);
  ble_commands::Command off;off.op=ble_commands::Op::Off;assert(ble_commands::execute(f.system,off)==ble_commands::Code::Ok);
  assert(!pump::isOn()&&f.ledger.record().total==1);pass();}
 {Fixture f;f.update(30000);const auto g=ble_command_bridge::connect(71);
  f.mem.during=[&]{ble_commands::Command off;off.op=ble_commands::Op::Off;
   assert(ble_command_bridge::reserve(g,1,off,ble_commands::Code::Ok,fakeNow));};
  assert(f.system.startManual(1)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.ledger.record().total==1);
  ble_command_bridge::serviceOwner(f.system);ble_command_bridge::disconnect(71);pass();}
 {Fixture f(false);f.update(30000);ble_commands::Command on;on.op=ble_commands::Op::On;on.seconds=1;
  assert(ble_commands::execute(f.system,on)==ble_commands::Code::StorageError);assert(!pump::isOn());pass();}

 {Fixture f;f.update(30000);f.mem.during=[&]{f.system.command("AUTO");};
  assert(f.system.startManual(1)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.ledger.record().total==1);pass();}
 {Fixture f;f.autoStart();f.update(1000,20,12.26f);assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::WaterUnsafe);
  auto consumed=f.ledger.record().total;f.update(30000,20,4);assert(!pump::isOn()&&f.ledger.record().total==consumed);
  f.update(2000,20,4);assert(pump::isOn()&&f.ledger.record().total==consumed+1);f.system.stopManual();pass();}
 {Fixture f;f.autoStart();f.update(5000,20,12.26f);assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::WaterUnsafe);pass();}
 {Fixture f;f.update(30000);f.mem.during=[&]{fakeNow+=4001;};
  assert(f.system.startManual(1)!=irrigation::Result::Accepted);assert(!pump::isOn()&&f.ledger.record().total==1);pass();}

 {Fixture f(false);f.update(30000);assert(f.system.command("RESET_WATERING")==irrigation::Result::Accepted);
  assert(!pump::isOn()&&f.ledger.record().total==0&&f.system.status().mode==irrigation::Mode::Manual);
  f.update(30000);assert(!pump::isOn());pass();}
 {Fixture f;f.update(30000);pump::testGuardFailure(false,true);
  f.mem.during=[&]{f.mem.failWrite=true;};assert(f.system.startManual(1)!=irrigation::Result::Accepted);
  assert(!pump::isOn());Ledger reboot(f.mem);f.mem.failWrite=false;
  assert(reboot.load(0)==Result::MaintenanceRequired);pump::testGuardFailure(false,false);pass();}

 {Fixture f;f.autoStart();f.mem.during=[&]{++epoch;};f.update(5000);
  assert(!pump::isOn()&&f.system.status().mode==irrigation::Mode::Manual);f.update(30000);assert(!pump::isOn());pass();}

 {Fixture f;f.autoStart();f.update(1000,80);assert(!pump::isOn()&&f.ledger.record().total==1);
  assert(f.system.status().decision.cooldownRemainingMs==30000);pass();}
 {Fixture f;f.update(30000);pump::testGuardFailure(false,true);f.system.startManual(1);pump::testGuardFailure(false,false);
  f.update(32000,80);assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::ActuatorFault);pass();}
 {Fixture f;f.autoStart();f.update(5000);f.update(30000);f.update(5000);f.update(1800000);f.update(5000);f.update(30000);f.update(5000);f.update(30000);
  Ledger reboot(f.mem);irrigation::System s;s.setActivationGate(capture,allowed,startReserved);s.begin(&f.config,&f.modes,&reboot);
  const uint32_t boot=fakeNow;for(unsigned i=0;i<=15;++i){fakeNow=boot+i*2000;s.update(f.sample());}
  assert(!pump::isOn()&&s.status().autonomousBlock==watering::Block::BudgetExhausted&&reboot.record().total==4);pass();}

 {Fixture f;fakeNow=UINT32_MAX-1000;Ledger reboot(f.mem);irrigation::System s;s.setActivationGate(capture,allowed,startReserved);
  s.begin(&f.config,&f.modes,&reboot);s.command("AUTO");const uint32_t boot=fakeNow;
  for(unsigned i=0;i<=15;++i){fakeNow=boot+i*2000;s.update(f.sample());}
  assert(pump::isOn()&&reboot.record().total==1);fakeNow+=5000;s.update(f.sample());assert(!pump::isOn());pass();}
 {Fixture f;f.autoStart();f.update(1000,80);f.mem.during=[&]{f.mem.failWrite=true;};f.update(32000,80);
  assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::StorageFault);
  assert(f.ledger.record().total==1);pass();}

 {Fixture f;f.update(30000);unsigned nestedWrites=0;
  f.mem.during=[&]{nestedWrites=f.mem.commits;assert(!pump::isOn());
   assert(f.system.startManual(1)==irrigation::Result::AlreadyOn);
   f.system.command("OFF");assert(!pump::isOn()&&f.mem.commits==nestedWrites);};
  assert(f.system.startManual(2)!=irrigation::Result::Accepted);
  assert(f.ledger.record().total==1&&f.system.status().mode==irrigation::Mode::Manual);pass();}
 {Fixture f;f.update(30000);plant_mqtt::commandConnection(true);
  f.mem.during=[&]{plant_mqtt::PumpCommand off;strcpy(off.id,"b32-disconnect-off");
   assert(plant_mqtt::queueCommand(off,fakeNow));plant_mqtt::commandConnection(false);fakeNow+=1100;};
  assert(f.system.startManual(1)!=irrigation::Result::Accepted);
  ble_command_bridge::serviceOwner(f.system);
  assert(!pump::isOn()&&f.ledger.record().total==1&&f.system.status().mode==irrigation::Mode::Manual);pass();}
 {Fixture f;f.update(30000);const auto g=ble_command_bridge::connect(95);
  ble_commands::Command on;on.op=ble_commands::Op::On;on.seconds=1;
  assert(ble_command_bridge::reserve(g,5,on,ble_commands::Code::Ok,fakeNow));
  f.mem.during=[&]{ble_command_bridge::disconnect(95);};ble_command_bridge::serviceOwner(f.system);
  // An already admitted command is allowed to finish; obsolete result is discarded.
  assert(pump::isOn()&&f.ledger.record().total==1);ble_commands::Dispatch::Slot result;
  assert(!ble_command_bridge::result(result));f.system.stopManual();pass();}
 {Fixture f;f.update(30000);const auto g=ble_command_bridge::connect(96);
  ble_commands::Command off;off.op=ble_commands::Op::Off;
  assert(ble_command_bridge::reserve(g,6,off,ble_commands::Code::Ok,fakeNow));
  ble_command_bridge::serviceOwner(f.system);assert(!pump::isOn()&&f.ledger.record().total==0);
  ble_command_bridge::disconnect(96);pass();}
 {Fixture f;f.update(30000);plant_mqtt::commandConnection(true);plant_mqtt::PumpCommand on;
  strcpy(on.id,"b32-delayed-ack");on.on=true;on.seconds=1;
  assert(plant_mqtt::queueCommand(on,fakeNow));int slot=-1;char ack[320];
  // Other completed fixture commands may still have delayed ACKs. None may
  // acknowledge THIS request before the owner has admitted it.
  for(unsigned i=0;i<16&&plant_mqtt::commandAck(fakeNow,slot,ack,sizeof(ack),"02:03:04:05:06:07");++i) {
    assert(!strstr(ack,"b32-delayed-ack"));plant_mqtt::ackAccepted(slot,fakeNow);
  }
  ble_command_bridge::serviceOwner(f.system);assert(pump::isOn());
  f.update(1000);assert(!pump::isOn());fakeNow+=1000;
  assert(plant_mqtt::queueCommand(on,fakeNow));ble_command_bridge::serviceOwner(f.system);
  assert(!pump::isOn()&&f.ledger.record().total==1);
  plant_mqtt::commandConnection(false);pass();}
 {Fixture f;f.autoStart();f.update(5000);f.update(30000);f.update(5000);
  f.system.stopManual();ble_commands::Command on;on.op=ble_commands::Op::On;on.seconds=1;
  assert(ble_commands::execute(f.system,on)!=ble_commands::Code::Ok);assert(!pump::isOn()&&f.ledger.record().total==2);pass();}

 {Fixture f(false);f.update(30000);plant_mqtt::commandConnection(true);plant_mqtt::PumpCommand on;
  strcpy(on.id,"b32-rejected-storage-ack");on.on=true;on.seconds=1;
  assert(plant_mqtt::queueCommand(on,fakeNow));ble_command_bridge::serviceOwner(f.system);
  int slot=-1;char ack[320];bool found=false;
  for(unsigned i=0;i<16&&plant_mqtt::commandAck(fakeNow,slot,ack,sizeof(ack),"02:03:04:05:06:07");++i){
   if(strstr(ack,"b32-rejected-storage-ack")){assert(strstr(ack,"FAILED")&&strstr(ack,"STORAGE_ERROR"));found=true;}
   plant_mqtt::ackAccepted(slot,fakeNow);
  }
  assert(found&&!pump::isOn()&&f.mem.commits==0);plant_mqtt::commandConnection(false);pass();}
 {Fixture f;f.update(30000);plant_mqtt::commandConnection(true);plant_mqtt::PumpCommand off,on;
  strcpy(off.id,"b32-off-priority");strcpy(on.id,"b32-on-after-off");on.on=true;on.seconds=1;
  assert(plant_mqtt::queueCommand(off,fakeNow));assert(!plant_mqtt::queueCommand(on,fakeNow));
  ble_command_bridge::serviceOwner(f.system);assert(!pump::isOn()&&f.ledger.record().total==0);
  plant_mqtt::commandConnection(false);pass();}
 // Actual owner integration: only a clean boot receives the DEMO retry.
 {Fixture f(true,true);f.autoStart();f.update(5000);f.update(29999);assert(!pump::isOn());
  f.update(1);assert(pump::isOn()&&f.ledger.record().total==2);f.update(5000);
  const uint32_t wait=WATERING_DEMO_RETRY?60000u:1800000u;
  f.update(wait-1);assert(!pump::isOn()&&f.ledger.record().total==2);
  f.update(1);assert(pump::isOn()&&f.ledger.record().total==3);
  f.update(5000);f.update(30000);assert(pump::isOn()&&f.ledger.record().total==4);
  f.update(5000);f.update(30000);assert(!pump::isOn()&&f.system.status().autonomousBlock==watering::Block::BudgetExhausted);pass();}
 {Fixture f(true,true);f.autoStart();f.update(5000);f.update(30000);f.update(5000);
  Ledger reboot(f.mem);irrigation::System system;system.setActivationGate(capture,allowed,startReserved);
  system.begin(&f.config,&f.modes,&reboot);const uint32_t boot=fakeNow;
  system.command("AUTO");
  for(uint32_t dt=2000;dt<=60000;dt+=2000){fakeNow=boot+dt;system.update(f.sample());}
  assert(!pump::isOn()&&reboot.record().total==2&&system.status().autonomousBlock==watering::Block::WaitingRetry);pass();}
 std::cout<<"PASS "<<cases<<" experimental owner integration scenarios (fake GPIO/NVS)\n";
}
