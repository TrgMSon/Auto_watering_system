#include "Arduino.h"
#include "../../esp32/irrigation.h"
#include "../../esp32/pump_serial.h"
#include <vector>
#include <iostream>
uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
struct Fake : configuration::Storage {
  std::vector<uint8_t> data;
  bool fail = false, readFail = false, corruptWrite = false; int saves = 0;
  bool read(uint8_t* b, size_t capacity, size_t& size) override {
    size = 0;
    if (readFail || data.empty() || data.size() > capacity) return false;
    size = data.size(); memcpy(b, data.data(), size); return true;
  }
  bool write(const uint8_t* b, size_t n) override {
    ++saves; if (fail) return false;
    data.assign(b,b+n); if (corruptWrite) data[10] ^= 1; return true;
  }
};
sensors::Reading sample(float soil = 20, float distance = 5) {
  sensors::Reading r; r.sampledAtMs=fakeNow; r.soilPercent=soil; r.soilValid=true;
  r.distanceCm=distance; r.distanceValid=true; return r;
}
int main() {
  using irrigation::Result;
  // Independent schema-1 fixture: five original floats and original FNV checksum.
  const uint8_t legacy[] = {
    0x43,0x46,0x47,0x34,0x01,0,0,0,0,0,0xc8,0x41,0,0,0x96,0x42,
    0,0,0xc8,0x41,0,0,0x20,0x40,0,0,0x40,0x41,0x5d,0x16,0x53,0x57};
  Fake migration; migration.data.assign(legacy, legacy + sizeof(legacy));
  irrigation::System migrated; migrated.begin(&migration);
  auto old = migrated.configurationStatus().active();
  assert(old.soilStart == 25 && old.soilStop == 75 && old.waterMin == 25 &&
      old.fullCm == 2.5f && old.emptyCm == 12 && old.maxRunSeconds == 120);
  assert(migration.saves == 0 && migrated.configurationStatus().persistedSchema() == 1);
  assert(migrated.status().mode == irrigation::Mode::Manual && outputLevel == LOW);
  assert(migrated.updateConfiguration(old) == configuration::Result::Saved);
  assert(migration.saves == 1 && migration.data.size() == 36 &&
      migrated.configurationStatus().persistedSchema() == 2);
  assert(configuration::get32(migration.data.data()+4) == 2);
  assert(configuration::get32(migration.data.data()+28) == 120);
  assert(memcmp(migration.data.data()+8, legacy+8, 20) == 0);
  // Every byte, including schema/duration/checksum, is protected from corruption.
  const auto validRecord = migration.data;
  for (size_t i = 0; i < validRecord.size(); ++i) {
    auto damaged = validRecord; damaged[i] ^= 1;
    auto unchanged = old; unchanged.soilStart = 26;
    const auto sentinel = unchanged;
    assert(!configuration::decode(damaged.data(), damaged.size(), unchanged));
    assert(configuration::equal(unchanged, sentinel));
  }
  assert(migrated.updateConfiguration(old) == configuration::Result::Unchanged && migration.saves == 1);
  for (uint32_t duration : {60u, 120u, 600u}) {
    auto c = old; c.maxRunSeconds = duration;
    assert(migrated.updateConfiguration(c) != configuration::Result::Invalid);
    migrated.begin(&migration);
    assert(migrated.configurationStatus().active().maxRunSeconds == duration);
    assert(migrated.status().mode == irrigation::Mode::Manual && !migrated.status().pumpOn);
  }
  for (uint32_t duration : {0u, 1u, 59u, 601u, UINT32_MAX}) {
    auto c = old; c.maxRunSeconds = duration; const auto saved = migration.data;
    assert(migrated.updateConfiguration(c) == configuration::Result::Invalid);
    assert(migration.data == saved && migrated.configurationStatus().active().maxRunSeconds == 600);
    // Valid checksum cannot authorize an invalid complete record.
    configuration::encode(c, migration.data.data());
    migrated.begin(&migration);
    assert(!migrated.configurationStatus().persisted() &&
        migrated.configurationStatus().active().maxRunSeconds == 120);
    migration.data = saved; migrated.begin(&migration);
  }
  // Reject legacy corruption even when length matches; decode never partially applies.
  for (unsigned fault = 0; fault < 4; ++fault) {
    migration.data.assign(legacy, legacy + sizeof(legacy));
    if (fault == 0) migration.data[0] ^= 1;
    if (fault == 1) migration.data[10] ^= 1;
    if (fault == 2) migration.data.push_back(0);
    if (fault == 3) migration.data.resize(37);
    const int saves = migration.saves; migrated.begin(&migration);
    assert(!migrated.configurationStatus().persisted() && migration.saves == saves);
  }
  migration.data.assign(legacy, legacy + sizeof(legacy)); migrated.begin(&migration);
  migration.fail = true;
  assert(migrated.updateConfiguration(old) == configuration::Result::StorageFailed);
  assert(configuration::equal(migrated.configurationStatus().active(), old));
  assert(migration.data.size() == 32); migration.fail = false;
  assert(migrated.updateConfiguration(old) == configuration::Result::Saved);
  auto changed = old; changed.maxRunSeconds = 600;
  migration.readFail = true;
  assert(migrated.updateConfiguration(changed) == configuration::Result::StorageFailed);
  assert(configuration::equal(migrated.configurationStatus().active(), old));
  migration.readFail = false;
  assert(migrated.updateConfiguration(old) == configuration::Result::Saved); // Repair flash after uncertain save.
  migration.corruptWrite = true;
  assert(migrated.updateConfiguration(changed) == configuration::Result::StorageFailed);
  assert(configuration::equal(migrated.configurationStatus().active(), old));
  migration.corruptWrite = false;
  assert(migrated.updateConfiguration(changed) == configuration::Result::Saved);
  migrated.update(sample()); migrated.command("ON");
  assert(migrated.updateConfiguration(old) == configuration::Result::Busy);
  fakeNow += 4999; migrated.update(sample()); assert(migrated.status().pumpOn);
  fakeNow += 1; migrated.update(sample()); assert(!migrated.status().pumpOn);
  const auto cooldown = migrated.status().decision;
  assert(migrated.updateConfiguration(old) == configuration::Result::Saved);
  assert(migrated.status().decision.state == cooldown.state &&
      migrated.status().decision.cooldownRemainingMs == cooldown.cooldownRemainingMs &&
      migrated.status().decision.wateringAttempts == cooldown.wateringAttempts);
  // A duration-only update cannot clear the third-attempt retry lock.
  migrated.begin(&migration); migrated.update(sample()); migrated.command("AUTO");
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    fakeNow += 5000; migrated.update(sample());
    if (attempt < 2) { fakeNow += 30000; migrated.update(sample()); }
  }
  migrated.command("MANUAL");
  const auto locked = migrated.status().decision;
  assert(locked.state == watering::State::Locked && locked.wateringAttempts == 3);
  changed = migrated.configurationStatus().active(); changed.maxRunSeconds = 600;
  assert(migrated.updateConfiguration(changed) == configuration::Result::Saved);
  assert(migrated.status().decision.state == locked.state &&
      migrated.status().decision.wateringAttempts == locked.wateringAttempts);
  migrated.service();
  assert(migrated.status().decision.reason == watering::Reason::RetryLimit);
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
    if (fault==3) { store.data[4]=3; configuration::put32(store.data.data()+32, configuration::checksum(store.data.data(),32)); }
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
