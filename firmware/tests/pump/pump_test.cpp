#include "Arduino.h"
#include "../../esp32/pump.h"
#include "../../esp32/pump_serial.h"
#include <iostream>
#include <limits>

uint32_t fakeNow = 0;
int outputLevel = -1, writes = 0, outputModes = 0;
using pump::Result;
int main() {
  pump::begin();
  assert(outputLevel == LOW && outputModes == 1 && !pump::isOn());
  assert(pump::remainingMs() == 0);
  assert(pump::command("STATUS") == Result::Status && outputLevel == LOW);
  assert(pump::command("ON") == Result::On && outputLevel == HIGH);
  assert(pump::remainingMs() == 5000);
  fakeNow = 4999;
  assert(pump::command("ON") == Result::AlreadyOn);
  assert(pump::remainingMs() == 1);
  fakeNow = 5000;
  assert(pump::command("ON") == Result::AlreadyOn && outputLevel == LOW);
  assert(!pump::isOn() && pump::remainingMs() == 0);
  assert(pump::command("ON") == Result::On);
  assert(pump::command("OFF") == Result::Off && outputLevel == LOW);
  assert(pump::command("OFF") == Result::Off);
  for (const char* bad : {"on", "ON 10", " ON", "", "START"}) {
    assert(pump::command(bad) == Result::Invalid && !pump::isOn());
  }
  assert(pump::command(nullptr) == Result::Invalid);
  pump::command("ON"); fakeNow += 7000;
  assert(pump::poll() && outputLevel == LOW);
  assert(!pump::poll());
  fakeNow = std::numeric_limits<uint32_t>::max() - 1000;
  pump::command("ON"); fakeNow = 3998;
  assert(pump::remainingMs() == 1 && outputLevel == HIGH);
  fakeNow = 3999;
  assert(pump::poll() && !pump::isOn());
  pump::SerialCommands parser;
  Result result = Result::Invalid;
  assert(!parser.feed('O', result)); assert(!parser.feed('N', result));
  assert(parser.feed('\r', result) && result == Result::On);
  assert(!parser.feed('\n', result));
  assert(!parser.feed('O', result)); assert(!parser.feed('N', result));
  assert(!parser.feed('\0', result));
  assert(parser.feed('\n', result) && result == Result::Invalid);
  assert(pump::isOn());
  for (int i = 0; i < 40; ++i) assert(!parser.feed('X', result));
  assert(parser.feed('\n', result) && result == Result::Invalid);
  for (char c : {'O','F','F'}) assert(!parser.feed(c, result));
  assert(parser.feed('\n', result) && result == Result::Off && !pump::isOn());
  // Reinitialization also forces OFF, with the latch loaded before pinMode.
  pump::command("ON"); pump::begin(); assert(!pump::isOn());
  std::cout << "PASS: safe init, commands, timeout, rollover, repeated ON, parser failures\n";
}
