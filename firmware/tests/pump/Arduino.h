#pragma once
#include <stdint.h>
#include <cassert>
#define LOW 0
#define HIGH 1
#define OUTPUT 2
extern uint32_t fakeNow;
extern int outputLevel, writes, outputModes;
inline uint32_t millis() { return fakeNow; }
inline void digitalWrite(int pin, int level) {
  assert(pin == 23); outputLevel = level; ++writes;
}
inline void pinMode(int pin, int mode) {
  assert(pin == 23 && mode == OUTPUT);
  assert(writes > 0 && outputLevel == LOW); ++outputModes;
}
