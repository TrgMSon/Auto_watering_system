#pragma once
#include "../../esp32/irrigation.h"
namespace test_mode {
class Storage : public desired_mode::Storage {
 public:
  uint8_t value=0;unsigned saves=0;
  bool readMode(uint8_t& out) override {out=value;return true;}
  bool writeMode(uint8_t in) override {value=in;++saves;return true;}
};
// Legacy safety/config suites start independent MANUAL test scenarios at each
// begin(). Inject explicit, working mode flash rather than allowing a production
// volatile fallback. Real AUTO reboot/failure behavior is tested separately.
class FreshSystem : public irrigation::System {
 public:
  using irrigation::System::System;
  void begin(configuration::Storage* config=nullptr) {
    modes=Storage{};irrigation::System::begin(config,&modes);
  }
  Storage modes;
};
}
