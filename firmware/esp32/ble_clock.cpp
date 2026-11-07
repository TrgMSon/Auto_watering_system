#include "ble_clock.h"
#ifdef ARDUINO
#include <sys/time.h>
#include <limits>
namespace ble_commands {
static_assert(std::numeric_limits<time_t>::is_signed && sizeof(time_t) >= 8,
    "TIME policy requires the installed IDF signed 64-bit time_t");
bool SystemClock::setUtc(uint32_t seconds) {
  if (!validUnix(seconds)) return false;
  timeval requested = {};
  requested.tv_sec = static_cast<time_t>(seconds);
  if (static_cast<uint64_t>(requested.tv_sec) != seconds || settimeofday(&requested, nullptr) != 0)
    return false;
  timeval actual = {};
  // Allow one tick of wall-clock progression between the two API calls.
  return gettimeofday(&actual, nullptr) == 0 && actual.tv_sec >= requested.tv_sec &&
      actual.tv_sec - requested.tv_sec <= 1 && actual.tv_usec >= 0 && actual.tv_usec < 1000000;
}
}
#endif
