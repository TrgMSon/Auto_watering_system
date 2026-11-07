// Execute the production adapter with fake system-call names. Never link/call
// the host settimeofday/gettimeofday functions and never change the host clock.
#include <sys/time.h>
#include <cassert>
#include <iostream>
#include <limits>
#include <stdint.h>
// Windows host timeval uses 32-bit long; model the verified IDF 64-bit ABI.
struct TargetTimeval { int64_t tv_sec=0, tv_usec=0; };
static TargetTimeval stored={};
static int setError=0, readError=0, setCalls=0, readCalls=0, delta=0;
static int fakeSet(const TargetTimeval* value, const void*) {
 ++setCalls; if(setError) return -1; stored=*value; return 0;
}
static int fakeGet(TargetTimeval* value, void*) {
 ++readCalls; if(readError) return -1; *value=stored; value->tv_sec+=delta; return 0;
}
#define ARDUINO 1
#define timeval TargetTimeval
#define time_t int64_t
#define settimeofday fakeSet
#define gettimeofday fakeGet
#include "../esp32/ble_clock.cpp"
#undef timeval
#undef time_t
#undef settimeofday
#undef gettimeofday
int main() {
 ble_commands::SystemClock clock;
 for(uint32_t utc : {ble_commands::kUnixMin,ble_commands::kUnixMax,1793800000u}) {
  assert(clock.setUtc(utc) && uint64_t(stored.tv_sec)==utc && stored.tv_usec==0);
 }
 int calls=setCalls;
 assert(!clock.setUtc(0) && !clock.setUtc(ble_commands::kUnixMax+1) && setCalls==calls);
 setError=1; calls=readCalls; assert(!clock.setUtc(ble_commands::kUnixMin) && readCalls==calls); setError=0;
 readError=1; assert(!clock.setUtc(ble_commands::kUnixMin)); readError=0;
 delta=-1; assert(!clock.setUtc(ble_commands::kUnixMin));
 delta=2; assert(!clock.setUtc(ble_commands::kUnixMin));
 delta=1; assert(clock.setUtc(ble_commands::kUnixMin));
 std::cout<<"PASS: production clock adapter bounds/set/readback with fake system calls; no host wall-clock access\n";
}
