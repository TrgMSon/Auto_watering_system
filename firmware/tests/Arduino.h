#pragma once
#include <stdint.h>
#include <algorithm>
#include <cassert>
#define LOW 0
#define HIGH 1
#define INPUT 1
#define OUTPUT 2
#define ADC_11db 3
#define constrain(x, a, b) std::min(std::max((x), (a)), (b))
extern uint32_t fakeNow, fakeEcho;
extern int fakeAdc, fakeEchoLevel, adcCalls, pulseCalls;
inline uint32_t millis() { return fakeNow; }
inline void checkPin(int pin) { assert(pin == 34 || pin == 27 || pin == 18 || pin == 19); }
inline void pinMode(int pin, int) { checkPin(pin); }
inline void digitalWrite(int pin, int) { checkPin(pin); assert(pin == 18); }
inline int digitalRead(int pin) { checkPin(pin); return fakeEchoLevel; }
inline void analogReadResolution(int bits) { assert(bits == 12); }
inline void analogSetPinAttenuation(int pin, int) { checkPin(pin); }
inline int analogRead(int pin) { checkPin(pin); ++adcCalls; return fakeAdc; }
inline void delayMicroseconds(int) {}
inline uint32_t pulseIn(int pin, int, uint32_t timeout) {
  checkPin(pin); assert(timeout == 30000); ++pulseCalls; return fakeEcho;
}
