#include "../esp32/watering.h"
#include <cassert>
#include <iostream>
using namespace watering;
static Config config() { Config c; c.fullDistanceCm=3; c.emptyDistanceCm=12; c.calibrationConfirmed=true; return c; }
struct Fixture {
  AutonomousController c{config()}; uint32_t now=0; uint64_t id=0;
  Fixture() { c.setAutomatic(true,now); }
  sensors::Reading sample(float soil=20) { sensors::Reading r; r.sampledAtMs=now; r.soilValid=r.distanceValid=true; r.soilPercent=soil; r.distanceCm=4; return r; }
  AutonomousDecision tick(uint32_t delta=0,float soil=20) { now+=delta; return c.evaluate(sample(soil),now,++id); }
  AutonomousDecision stable(float soil=20) { AutonomousDecision d=tick(0,soil); for(int i=0;i<15;++i)d=tick(2000,soil); return d; }
};
static unsigned cases=0;
static void pass(){++cases;}
int main(){
 { Fixture f; auto d=f.tick(); assert(d.desiredPumpOn&&d.cycle==1&&d.reservationsTotal==1); pass(); }
 { Fixture f; auto d=f.tick(0,70); assert(!d.desiredPumpOn&&!d.episodeActive); pass(); }
 { Fixture f; f.tick(); assert(f.tick(4999).desiredPumpOn); auto d=f.tick(1); assert(!d.desiredPumpOn&&d.state==State::Cooldown); pass(); }
 { Fixture f; f.tick(); auto d=f.tick(1000,70); assert(!d.desiredPumpOn&&d.reservationsTotal==1); assert(!f.tick(29999).desiredPumpOn); assert(f.tick(1).desiredPumpOn); pass(); }
 { Fixture f; f.tick(); f.tick(5000); auto r=f.sample(); f.now+=30000; r.sampledAtMs=f.now-1;
   auto d=f.c.evaluate(r,f.now,++f.id); assert(!d.desiredPumpOn); // acquired after cooldown: 29999ms only
   d=f.c.evaluate(r,f.now+1,f.id); assert(!d.desiredPumpOn); assert(f.tick(1).desiredPumpOn); pass(); }
 { Fixture f; f.tick(); auto r=f.sample(); for(int i=0;i<50;++i)assert(f.c.evaluate(r,f.now,f.id).reservationsTotal==1); pass(); }
 { Fixture f; f.tick(); f.tick(5000); f.tick(30000); auto d=f.tick(5000); assert(d.reservationsTotal==2);
   d=f.tick(30000); assert(d.block==Block::WaitingRetry&&!d.desiredPumpOn);
   d=f.tick(1769999); assert(d.block==Block::WaitingRetry); d=f.tick(1); assert(d.desiredPumpOn&&d.cycle==2&&d.reservationsTotal==3);
   f.tick(5000); f.tick(30000); d=f.tick(5000); f.tick(30000); d=f.tick(1800000);
   assert(d.block==Block::BudgetExhausted&&d.reservationsTotal==4); pass(); }
 { Fixture f; f.tick(); f.tick(5000); f.tick(30000); f.tick(5000); f.tick(1800000); f.tick(5000); f.tick(30000); f.tick(5000); f.tick(30000);
   auto d=f.stable(70); assert(!d.episodeActive&&d.reservationsTotal==0&&!d.desiredPumpOn); assert(f.tick(2000).reservationsTotal==1); pass(); }
 { Fixture f; f.tick(); f.tick(1000,70); f.tick(2000,70); f.tick(2000,69); auto d=f.tick(2000,70);
   assert(d.reservationsTotal==1); for(int i=0;i<14;++i)d=f.tick(2000,70); assert(d.reservationsTotal==1);
   d=f.tick(2000,70); assert(d.reservationsTotal==0); pass(); }
 { Fixture f; f.tick(); f.tick(1000,70); auto r=f.sample(70); for(int i=0;i<30;++i) f.c.evaluate(r,f.now,f.id);
   f.now+=30000; auto d=f.c.evaluate(r,f.now,f.id); assert(d.block==Block::SensorUnsafe&&d.reservationsTotal==1); pass(); }
 { Fixture f; f.tick(); f.now+=1000; auto r=f.sample(); r.soilValid=false;
   auto d=f.c.evaluate(r,f.now,++f.id); assert(d.block==Block::SensorUnsafe&&!d.desiredPumpOn);
   d=f.tick(2000); assert(d.block==Block::SensorUnsafe); for(int i=0;i<14;++i)d=f.tick(2000);
   assert(d.block==Block::SensorUnsafe); d=f.tick(2000); assert(d.desiredPumpOn&&d.reservationsTotal==2); pass(); }
 { Fixture f; f.tick(); f.now+=5000; auto r=f.sample(); r.distanceCm=12;
   auto d=f.c.evaluate(r,f.now,++f.id); assert(d.block==Block::WaterUnsafe&&!d.desiredPumpOn&&d.reservationsTotal==1); pass(); }
 { Fixture f; f.tick(); f.now+=1000; f.c.setAutomatic(false,f.now); auto d=f.tick();
   assert(!d.desiredPumpOn&&!d.automatic&&d.reservationsTotal==1);
   d=f.stable(); assert(!d.desiredPumpOn&&!d.automatic); f.c.setAutomatic(true,f.now); d=f.tick(); assert(d.desiredPumpOn&&d.reservationsTotal==2); pass(); }
 for(Block b:{Block::ActuatorFault,Block::StorageFault}) { Fixture f; f.tick(); f.c.latchFault(b,f.now);
   auto d=f.stable(80); assert(!d.desiredPumpOn&&d.block==b&&d.reservationsTotal==1); pass(); }
 { Fixture f; f.now=UINT32_MAX-1000; f.tick(); f.tick(5000); auto d=f.tick(29999); assert(!d.desiredPumpOn); d=f.tick(1); assert(d.desiredPumpOn&&d.reservationsTotal==2); pass(); }
 { Fixture f; f.tick(); f.tick(1000,70); f.now+=5000; auto r=f.sample(70); r.soilValid=false; f.c.evaluate(r,f.now,++f.id);
   auto d=f.stable(70); assert(!d.desiredPumpOn&&d.reservationsTotal==0); pass(); }

 { Fixture f; f.tick(); f.now+=1000; auto r=f.sample(); r.distanceValid=false;
   auto d=f.c.evaluate(r,f.now,++f.id); assert(d.block==Block::SensorUnsafe);
   for(int i=0;i<10;++i) { d=f.tick(2000); }
   r=f.sample(); r.distanceCm=12; f.c.evaluate(r,f.now,++f.id);
   d=f.tick(2000); for(int i=0;i<14;++i)d=f.tick(2000); assert(d.block==Block::WaterUnsafe);
   d=f.tick(2000); assert(d.reservationsTotal==2&&d.desiredPumpOn); pass(); }
 { Fixture f; f.tick(); f.tick(1000,70); f.tick(2000,70); f.tick(5000,70);
   auto d=f.tick(28000,70); assert(d.reservationsTotal==1); // acquisition gap broke continuity
   pass(); }
 { Fixture f; f.tick(); f.tick(1000); f.c.setAutomatic(false,f.now); f.tick(30000);
   f.c.configure(config()); auto d=f.tick(); assert(!d.automatic&&d.reservationsTotal==1); pass(); }
 { Fixture f; f.now=UINT32_MAX-10000; f.tick(); f.tick(5000); f.tick(30000); f.tick(5000);
   f.c.setAutomatic(false,f.now); auto d=f.tick(1800000); assert(!d.desiredPumpOn&&d.reservationsTotal==2);
   f.c.setAutomatic(true,f.now); d=f.tick(); assert(d.desiredPumpOn&&d.cycle==2); pass(); }
 { Fixture f; f.tick(); f.tick(1000,70); f.tick(30000,70); // gap cannot complete wet confirmation
   auto r=f.sample(70); auto d=f.c.evaluate(r,f.now,0); assert(d.reservationsTotal==1);
   d=f.stable(70); assert(d.reservationsTotal==0); pass(); }

 { Fixture f; f.tick(); f.tick(5000); f.tick(30000); f.tick(5000); f.tick(1800000); f.tick(5000); f.tick(30000); f.tick(5000); f.tick(30000);
   f.c.setAutomatic(false,f.now); f.c.setAutomatic(true,f.now); f.c.configure(config());
   auto d=f.tick(1800000); assert(d.block==Block::BudgetExhausted&&d.reservationsTotal==4&&!d.desiredPumpOn); pass(); }
 std::cout<<"PASS "<<cases<<" autonomous controller scenarios (RAM-only, no pump integration)\n";
}
