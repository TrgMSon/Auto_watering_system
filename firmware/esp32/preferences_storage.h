#pragma once
#include <Preferences.h>
#include "configuration.h"

namespace configuration {
class PreferencesStorage : public Storage {
 public:
  bool read(uint8_t* bytes, size_t capacity, size_t& actualSize) override {
    actualSize = 0;
    Preferences prefs;
    if (!prefs.begin("watering_cfg", true)) return false;
    const size_t size = prefs.getBytesLength("record");
    const bool ok = size > 0 && size <= capacity &&
        prefs.getBytes("record", bytes, size) == size;
    if (ok) actualSize = size;
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
