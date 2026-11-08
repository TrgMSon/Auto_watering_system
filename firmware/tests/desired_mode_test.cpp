#include "pump/Arduino.h"
#include "../esp32/irrigation.h"
#include "../esp32/mqtt_commands.h"
#include "../esp32/ble_command_bridge.h"
#include <iostream>
#include <string>
#include <vector>
uint32_t fakeNow=0;
int outputLevel=-1,writes=0,outputModes=0;
using irrigation::Mode;
using irrigation::Result;
struct Cut {};
struct Flash : configuration::Storage, desired_mode::Storage {
  uint8_t mode=0xff;bool present=false,readError=false;
  std::vector<uint8_t> blob;
  std::vector<std::string> trace;
  unsigned operations=0,failAt=0,cutAt=0,modeWrites=0,configWrites=0;
  bool failAfter=false,corruptRead=false;
  irrigation::System* owner=nullptr;
  template<class F> bool operation(const std::string& label,F action){
    trace.push_back(label);++operations;
    if(operations==failAt&&!failAfter)return false;
    const bool result=action();if(operations==cutAt)throw Cut{};
    if(operations==failAt&&failAfter)return false;
    return result;
  }
  void checkOff(){
    assert(outputLevel==LOW&&!pump::isOn());
    if(owner)assert(owner->status().mode==Mode::Manual&&!owner->status().pumpOn);
  }
  bool readMode(uint8_t& out)override{
    return operation("read-mode",[&](){out=corruptRead?0xff:mode;return present&&!readError;});
  }
  bool writeMode(uint8_t value)override{
    checkOff();++modeWrites;
    return operation(value?"write-AUTO":"write-MANUAL",[&](){mode=value;present=true;return true;});
  }
  bool read(uint8_t* out,size_t capacity,size_t& size)override{
    return operation("read-config",[&](){
      size=0;if(blob.empty()||blob.size()>capacity)return false;
      size=blob.size();memcpy(out,blob.data(),size);return true;
    });
  }
  bool write(const uint8_t* in,size_t size)override{
    checkOff();++configWrites;
    return operation("write-config",[&](){blob.assign(in,in+size);return true;});
  }
  void seed(bool automatic){
    mode=automatic?1:0;present=true;configuration::Config config;
    blob.resize(configuration::kRecordSize);configuration::encode(config,blob.data());
  }
  void clearTrace(){operations=failAt=cutAt=0;failAfter=false;trace.clear();}
};
sensors::Reading sample(float soil=80,float distance=5){
  sensors::Reading r;r.sampledAtMs=fakeNow;r.soilValid=r.distanceValid=true;
  r.soilPercent=soil;r.distanceCm=distance;return r; // DHT invalid deliberately.
}
void boot(){
  Flash flash;flash.seed(true);irrigation::System system;flash.owner=&system;
  fakeNow=100;system.begin(&flash,&flash);
  assert(!system.status().pumpOn&&system.status().mode==Mode::Manual&&system.status().pendingBootAuto);
  assert(system.status().desiredMode==Mode::Auto&&system.status().modePersisted);
  for(unsigned i=0;i<10;++i)system.service();
  assert(system.status().pendingBootAuto&&flash.modeWrites==0);
  const auto pendingConfig=system.configurationStatus().active();
  assert(system.updateConfiguration(pendingConfig)==configuration::Result::Busy&&flash.configWrites==0);
  auto old=sample();old.sampledAtMs=99;system.update(old);assert(system.status().pendingBootAuto);
  auto invalid=sample();invalid.soilValid=false;system.update(invalid);assert(system.status().pendingBootAuto);
  invalid=sample();invalid.distanceValid=false;system.update(invalid);assert(system.status().pendingBootAuto);
  invalid=sample();fakeNow+=4001;system.update(invalid);assert(system.status().pendingBootAuto);
  system.update(sample(20,12.26f));assert(system.status().pendingBootAuto&&!system.status().pumpOn);
  system.update(sample(80));assert(!system.status().pendingBootAuto&&system.status().mode==Mode::Auto&&!system.status().pumpOn);
  assert(flash.modeWrites==0); // Activation never writes NVS.
  system.begin(&flash,&flash);system.update(sample(20));
  assert(system.status().mode==Mode::Auto&&system.status().pumpOn&&pump::run().durationMs==5000);
  const auto run=pump::run();const auto beforeRepeat=flash.modeWrites;
  system.command("AUTO");assert(flash.modeWrites==beforeRepeat&&pump::run().startedMs==run.startedMs);
  system.begin(&flash,&flash);
  assert(system.status().pendingBootAuto&&!system.status().pumpOn&&system.status().remainingMs==0&&
      system.status().decision.wateringAttempts==0&&system.status().decision.state==watering::State::Idle);
  system.update(sample(20));assert(system.status().pumpOn); // New run, not resumed deadline/state.
  system.stopManual();assert(flash.mode==0);const unsigned saves=flash.modeWrites;
  system.command("MANUAL");system.stopManual();assert(flash.modeWrites==saves);
  system.begin(&flash,&flash);system.update(sample(20));assert(system.status().mode==Mode::Manual&&!system.status().pumpOn);
  for(unsigned fault=0;fault<3;++fault){
    flash.seed(true);flash.present=fault!=0;flash.mode=fault==1?2:1;flash.readError=fault==2;
    system.begin(&flash,&flash);system.update(sample(20));
    assert(system.status().mode==Mode::Manual&&!system.status().pendingBootAuto&&!system.status().pumpOn);
  }
  flash.readError=false;flash.seed(true);
  auto config=irrigation::confirmedConfig();config.calibrationConfirmed=false;
  irrigation::System uncalibrated(config);flash.owner=&uncalibrated;
  uncalibrated.begin(nullptr,&flash);uncalibrated.update(sample(20));
  assert(uncalibrated.status().pendingBootAuto&&!uncalibrated.status().pumpOn);
  // Acquisition and freshness checks remain valid across millis rollover.
  flash.owner=&system;fakeNow=UINT32_MAX-10;system.begin(&flash,&flash);fakeNow=5;
  system.update(sample());assert(system.status().mode==Mode::Auto&&!system.status().pumpOn);
}
void transitions(){
  Flash flash;flash.seed(false);irrigation::System system;flash.owner=&system;
  system.begin(&flash,&flash);system.update(sample());flash.clearTrace();
  assert(system.command("AUTO")==Result::Accepted&&flash.mode==1);
  const unsigned saves=flash.modeWrites;system.command("AUTO");system.service();assert(flash.modeWrites==saves);
  assert(system.command("RESET_WATERING")==Result::Accepted&&flash.mode==0);
  assert(system.status().mode==Mode::Manual);
  system.command("AUTO");system.command("MANUAL");assert(flash.mode==0);
  // No backing mode storage: AUTO must fail closed, never fall back to volatile.
  irrigation::System absent;absent.begin();absent.update(sample(20));
  assert(absent.command("AUTO")==Result::ModeStorageFailed&&!absent.status().pumpOn&&absent.status().mode==Mode::Manual);
  assert(absent.command("RESET_WATERING")==Result::ModeStorageFailed&&!absent.status().pumpOn);
  flash.owner=&system;system.begin(&flash,&flash);system.update(sample(20));flash.clearTrace();flash.failAt=1;
  assert(system.command("AUTO")==Result::ModeStorageFailed&&!system.status().pumpOn);
  assert(system.status().mode==Mode::Manual&&system.status().modeStorageError);
  const auto operations=flash.operations;flash.failAt=0;system.update(sample(20));system.service();
  assert(flash.operations==operations&&!system.status().pumpOn); // No auto-retry or late activation.
  for(bool readback:{false,true}){
    flash.seed(true);flash.clearTrace();system.begin(&flash,&flash);system.update(sample(20));assert(system.status().pumpOn);
    flash.clearTrace();flash.failAt=readback?2:1;
    assert(system.stopManual()==Result::Accepted); // OFF succeeded, persistence did not.
    assert(outputLevel==LOW&&system.status().mode==Mode::Manual&&!system.status().pendingBootAuto&&system.status().modeStorageError);
    const auto count=flash.operations;flash.failAt=0;system.update(sample(20));system.service();assert(flash.operations==count&&!system.status().pumpOn);
    // Failed write can leave old AUTO in flash. Boot may pend AUTO, never resume ON.
    system.begin(&flash,&flash);assert(!system.status().pumpOn&&system.status().pendingBootAuto==!readback);
  }
  flash.seed(false);flash.clearTrace();system.begin(&flash,&flash);system.update(sample());
  flash.corruptRead=true;assert(system.command("AUTO")==Result::ModeStorageFailed&&!system.status().pumpOn);
  flash.corruptRead=false;
  system.begin(&flash,&flash);assert(system.status().pendingBootAuto); // Write happened; readback was invalid.
  flash.clearTrace();system.command("OFF");assert(!system.status().pendingBootAuto&&flash.mode==0);
  // A Serial SET after failed OFF must confirm MANUAL before touching config.
  flash.seed(true);system.begin(&flash,&flash);system.update(sample());flash.clearTrace();flash.failAt=1;
  system.command("OFF");assert(flash.mode==1&&system.status().mode==Mode::Manual);
  flash.clearTrace();flash.failAt=1;const auto configWrites=flash.configWrites;
  assert(system.command("SET SOIL_START 35")==Result::ConfigFailed&&flash.configWrites==configWrites);
  assert(!system.status().pumpOn&&system.status().modeStorageError);
}
void configOrdering(){
  const std::vector<std::string> order={"write-MANUAL","read-mode","write-config","read-config","write-AUTO","read-mode"};
  // Fail before and after each operation: RAM safety, flash uncertainty, and
  // reboot state follow ordering rather than any claim of two-key atomicity.
  for(unsigned boundary=1;boundary<=6;++boundary)for(bool after:{false,true}){
    Flash flash;flash.seed(true);irrigation::System system;flash.owner=&system;
    system.begin(&flash,&flash);system.update(sample());flash.clearTrace();
    auto candidate=system.configurationStatus().active();candidate.soilStart=35;
    const auto old=system.configurationStatus().active();flash.failAt=boundary;flash.failAfter=after;
    assert(system.applyRemoteConfiguration(candidate,true)==irrigation::RemoteConfigResult::StorageError);
    assert(system.status().mode==Mode::Manual&&!system.status().pendingBootAuto&&!system.status().pumpOn);
    if(boundary<=4)assert(configuration::equal(old,system.configurationStatus().active()));
    else assert(configuration::equal(candidate,system.configurationStatus().active()));
    if(boundary<=2)assert(flash.configWrites==0);
    const unsigned ops=flash.operations;flash.failAt=0;system.update(sample(20));system.service();assert(ops==flash.operations&&!system.status().pumpOn);
    system.begin(&flash,&flash);assert(!system.status().pumpOn);
    assert(system.status().pendingBootAuto==(flash.mode==1));
    assert(system.configurationStatus().active().soilStart==(boundary>=3&&(boundary!=3||after)?35:30));
  }
  for(unsigned cut=1;cut<=6;++cut){
    Flash flash;flash.seed(true);irrigation::System system;flash.owner=&system;
    system.begin(&flash,&flash);system.update(sample());flash.clearTrace();flash.cutAt=cut;
    auto candidate=system.configurationStatus().active();candidate.soilStart=35;
    try{system.applyRemoteConfiguration(candidate,true);assert(false);}catch(const Cut&){}
    assert(outputLevel==LOW);flash.cutAt=0;
    system.begin(&flash,&flash);
    assert(!system.status().pumpOn&&system.status().pendingBootAuto==(cut>=5));
    assert(system.configurationStatus().active().soilStart==(cut>=3?35:30));
  }
  Flash flash;flash.seed(true);irrigation::System system;flash.owner=&system;
  system.begin(&flash,&flash);system.update(sample());flash.clearTrace();
  auto candidate=system.configurationStatus().active();candidate.soilStart=35;
  assert(system.applyRemoteConfiguration(candidate,true)==irrigation::RemoteConfigResult::Ok&&flash.trace==order);
  flash.clearTrace();assert(system.applyRemoteConfiguration(candidate,true)==irrigation::RemoteConfigResult::Ok&&flash.trace.empty());
  candidate.soilStart=75;candidate.soilStop=75;
  assert(system.applyRemoteConfiguration(candidate,false)==irrigation::RemoteConfigResult::Invalid&&flash.trace.empty()&&system.status().mode==Mode::Auto);
  // Busy rejects before any mode/config write.
  system.update(sample(20));assert(system.status().pumpOn);candidate.soilStart=35;
  assert(system.applyRemoteConfiguration(candidate,false)==irrigation::RemoteConfigResult::PumpRunning&&flash.trace.empty());
  system.stopManual();
}
void adapters(){
  using namespace plant_mqtt;
  Flash flash;flash.seed(true);irrigation::System system;flash.owner=&system;
  system.begin(&flash,&flash);system.update(sample(20));flash.clearTrace();flash.failAt=1;
  ble_commands::Command off;off.op=ble_commands::Op::Off;
  assert(ble_commands::execute(system,off)==ble_commands::Code::Ok);
  assert(!system.status().pumpOn&&system.status().modeStorageError); // No new BLE result code.
  flash.failAt=0;flash.seed(true);system.begin(&flash,&flash);system.update(sample(20));
  commandConnection(true);PumpCommand mqttOff;strcpy(mqttOff.id,"mode-off");
  assert(queueCommand(mqttOff,fakeNow));flash.clearTrace();flash.failAt=1;ble_command_bridge::serviceOwner(system);
  char bytes[320];int slot=-1;assert(commandAck(fakeNow,slot,bytes,sizeof(bytes),"02:03:04:05:06:07"));
  assert(strstr(bytes,"\"status\":\"SUCCESS\"")&&strstr(bytes,"OFF_APPLIED_MODE_SAVE_FAILED"));
  assert(!system.status().pumpOn&&system.status().modeStorageError);
  const unsigned saves=flash.modeWrites;assert(queueCommand(mqttOff,fakeNow));ble_command_bridge::serviceOwner(system);assert(flash.modeWrites==saves);
  // CONFIG mode write failure is a FAILED CONFIG ACK, never the OFF mapping.
  flash.failAt=0;system.begin(&flash,&flash);system.update(sample());
  PumpCommand config;config.isConfig=true;config.values[0]=35;config.values[1]=75;config.values[2]=20;config.values[3]=2;
  config.values[4]=50;config.values[5]=80;config.values[6]=20;config.values[7]=35;config.automatic=true;strcpy(config.id,"config-mode-failure");
  assert(queueCommand(config,fakeNow));flash.clearTrace();flash.failAt=1;ble_command_bridge::serviceOwner(system);
  ackAccepted(slot,fakeNow);
  assert(commandAck(fakeNow,slot,bytes,sizeof(bytes),"02:03:04:05:06:07"));
  assert(strstr(bytes,"\"status\":\"FAILED\"")&&strstr(bytes,"\"action\":\"CONFIG\"")&&strstr(bytes,"STORAGE_ERROR"));
}
int main(){boot();transitions();configOrdering();adapters();std::cout<<"PASS desired mode boot/acquisition, persistence/OFF, all config boundaries and adapters (host mocks only)\n";}
