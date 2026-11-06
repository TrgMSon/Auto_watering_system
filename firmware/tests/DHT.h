#pragma once
#include "Arduino.h"
#define DHT11 11
extern float fakeTemperature, fakeHumidity;
extern int dhtCalls;
class DHT {
 public:
  DHT(int pin, int type) { checkPin(pin); assert(type == DHT11); }
  void begin() {}
  float readTemperature() { ++dhtCalls; return fakeTemperature; }
  float readHumidity() { return fakeHumidity; }
};
