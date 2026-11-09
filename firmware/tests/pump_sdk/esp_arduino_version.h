#pragma once
#define ESP_ARDUINO_VERSION_VAL(a,b,c) (((a)<<16)|((b)<<8)|(c))
#ifndef ESP_ARDUINO_VERSION
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(3,3,12)
#endif
