#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include "../../esp32/pump_serial.h"
#include <vector>
#include <iostream>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
struct Fake : configuration::Storage {
  std::vector<uint8_t> data;
  bool fail = false; int saves = 0;
  bool read(uint8_t* b, size_t n) override {
    if (data.size() != n) return false;
    memcpy(b, data.data(), n); return true;
  }
  bool write(const uint8_t* b, size_t n) override {
    ++saves; if (fail) return false;
    data.assign(b,b+n); return true;
  }
};
sensors::Reading sample(float soil = 20, float distance = 5) {
  sensors::Reading r; r.sampledAtMs=fakeNow; r.soilPercent=soil; r.soilValid=true;
  r.distanceCm=distance; r.distanceValid=true; return r;
}
int main() {
  using irrigation::Result;
  Fake store; irrigation::System s; s.begin(&store);
  assert(configuration::valid(configuration::Config{}));
  assert(!s.configurationStatus().persisted() && store.saves == 0);
  assert(s.status().mode == irrigation::Mode::Manual && outputLevel == LOW);
  for (const char* bad : {"SET SOIL_START nan", "SET SOIL_STOP inf", "SET WATER_MIN 19",
      "SET SOIL_START 70", "SET SOIL_STOP 30", "SET SOIL_START -1", "SET SOIL_STOP 101",
      "SET FULL_CM 1.9", "SET FULL_CM 12.26", "SET EMPTY_CM 13", "SET EMPTY_CM 3",
      "SET FULL_CM 4", "SET SOIL_START 31x", "SET SOIL_START 31 ", "SET SOIL_START  31",
      "SET SOIL_START 1e999", "SET SOIL_START 0x1p2", "SET SOIL_START 1e-999",
      "SET SOIL_START ", "SET UNKNOWN 4"}) {
    assert(s.command(bad) == Result::Invalid);
    assert(s.configurationStatus().active().soilStart == 30 && store.saves == 0);
  }
  for (const char* good : {"SET SOIL_START 35", "SET SOIL_STOP 75", "SET WATER_MIN 25",
                           "SET FULL_CM 2.5", "SET EMPTY_CM 12"})
    assert(s.command(good) == Result::ConfigSaved);
  assert(store.saves == 5);
  assert(s.command("SET EMPTY_CM 12") == Result::ConfigUnchanged && store.saves == 5);
  irrigation::System reboot; reboot.begin(&store);
  assert(reboot.configurationStatus().active().soilStart == 35);
  assert(reboot.configurationStatus().persisted() && outputLevel == LOW);
  store.fail = true;
  assert(s.command("SET SOIL_START 36") == Result::ConfigFailed);
  assert(s.command("RESET_CONFIG") == Result::ConfigFailed);
  assert(s.configurationStatus().active().soilStart == 35);
  store.fail = false;
  s.update(sample(32)); assert(s.command("AUTO") == Result::Accepted && s.status().pumpOn);
  assert(s.command("RESET_CONFIG") == Result::ConfigBusy);
  s.command("MANUAL");
  assert(s.status().decision.state == watering::State::Locked);
  assert(s.command("RESET_CONFIG") == Result::ConfigSaved);
  assert(s.status().decision.state == watering::State::Locked);
  assert(s.status().decision.wateringAttempts == 1 && !s.status().pumpOn);
  assert(s.command("RESET_CONFIG") == Result::ConfigUnchanged);
  s.begin(&store); s.update(sample()); s.command("ON");
  assert(s.command("SET SOIL_START 35") == Result::ConfigBusy);
  s.command("OFF"); s.begin(&store); s.update(sample(50)); s.command("AUTO");
  assert(!s.status().pumpOn && s.command("SET SOIL_START 35") == Result::ConfigBusy);
  // Calibration and minimum water affect manual safety immediately.
  s.command("MANUAL"); s.update(sample(20, 10));
  assert(s.command("SET WATER_MIN 50") == Result::ConfigSaved);
  assert(s.command("ON") == Result::Unsafe);
  assert(s.command("SET WATER_MIN 20") == Result::ConfigSaved);
  assert(s.command("SET EMPTY_CM 10") == Result::ConfigSaved);
  assert(s.command("ON") == Result::Unsafe);
  assert(s.command("RESET_CONFIG") == Result::ConfigSaved);
  assert(s.command("ON") == Result::Accepted);
  // Cooldown and counters survive configuration updates.
  fakeNow += 5000; s.update(sample());
  auto before = s.status().decision;
  assert(s.command("SET SOIL_START 35") == Result::ConfigSaved);
  assert(s.status().decision.state == watering::State::Cooldown);
  assert(s.status().decision.cooldownRemainingMs == before.cooldownRemainingMs);
  // Missing/truncated/checksum/schema/invalid payload all fall back without writes.
  auto saved = store.data; int saves = store.saves;
  for (int fault=0; fault<5; ++fault) {
    store.data=saved;
    if (fault==0) store.data.clear();
    if (fault==1) store.data.pop_back();
    if (fault==2) store.data[10]^=1;
    if (fault==3) { store.data[4]=2; uint32_t h=configuration::checksum(store.data.data()); memcpy(store.data.data()+28,&h,4); }
    if (fault==4) { configuration::Config c; c.waterMin=0; configuration::encode(c,store.data.data()); }
    reboot.begin(&store);
    assert(!reboot.configurationStatus().persisted());
    assert(reboot.configurationStatus().active().soilStart == 30);
    assert(outputLevel == LOW && store.saves == saves);
  }
  pump::SerialCommands parser; const char* line=nullptr;
  for (const char* p="SET SOIL_START 35"; *p; ++p) assert(!parser.feedLine(*p,line));
  assert(parser.feedLine('\n',line) && strcmp(line,"SET SOIL_START 35")==0);
  for (int i=0;i<80;++i) parser.feedLine('X',line);
  assert(parser.feedLine('\n',line) && line==nullptr);
  std::cout << "PASS: F2.4 configuration persistence, validation, safety gates and runtime integration\n";
}
