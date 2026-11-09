#pragma once
#include <stdint.h>
#include <cassert>
#define IRAM_ATTR
#define DRAM_ATTR
#define LOW 0
#define HIGH 1
#define OUTPUT 2
struct portMUX_TYPE { int unused; };
#define portMUX_INITIALIZER_UNLOCKED {0}
extern int sdkLockDepth;
inline void enterCritical(portMUX_TYPE*) { ++sdkLockDepth; }
inline void exitCritical(portMUX_TYPE*) { assert(sdkLockDepth>0);--sdkLockDepth; }
#define portENTER_CRITICAL(p) enterCritical(p)
#define portEXIT_CRITICAL(p) exitCritical(p)
#define portENTER_CRITICAL_ISR(p) enterCritical(p)
#define portEXIT_CRITICAL_ISR(p) exitCritical(p)
void pinMode(int,int);
void fakePinWrite(int,int);
