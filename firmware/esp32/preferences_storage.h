#pragma once
#include <Preferences.h>
#include "configuration.h"
#include "desired_mode.h"

namespace configuration {
class PreferencesStorage : public Storage, public desired_mode::Storage {
 public:
  bool readMode(uint8_t& value) override {
    Preferences prefs;
    if (!prefs.begin("watering_cfg", true)) return false;
    const bool typed = prefs.getType("mode") == PT_U8;
    value = typed ? prefs.getUChar("mode", 0xff) : 0xff;
    prefs.end(); return typed && value <= 1;
  }
  bool writeMode(uint8_t value) override {
    if (value > 1) return false;
    Preferences prefs;
    if (!prefs.begin("watering_cfg", false)) return false;
    const bool ok = prefs.putUChar("mode", value) == sizeof(value);
    prefs.end(); return ok;
  }
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
