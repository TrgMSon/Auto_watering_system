#pragma once
#include <Preferences.h>
#include "configuration.h"

namespace configuration {
class PreferencesStorage : public Storage {
 public:
  bool read(uint8_t* bytes, size_t size) override {
    Preferences prefs;
    if (!prefs.begin("watering_cfg", true)) return false;
    const bool ok = prefs.getBytesLength("record") == size &&
        prefs.getBytes("record", bytes, size) == size;
    prefs.end(); return ok;
  }
  bool write(const uint8_t* bytes, size_t size) override {
    Preferences prefs;
    if (!prefs.begin("watering_cfg", false)) return false;
    const bool ok = prefs.putBytes("record", bytes, size) == size;
    prefs.end(); return ok;
  }
};
}
