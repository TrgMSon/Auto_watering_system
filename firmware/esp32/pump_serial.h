#pragma once
#include <stddef.h>
#include "pump.h"

namespace pump {
// Fixed-size, nonblocking line parser. Overlong lines are discarded in full.
class SerialCommands {
 public:
  bool feed(char c, Result& result) {
    const char* line = nullptr;
    if (!feedLine(c, line)) return false;
    result = line ? command(line) : Result::Invalid;
    return true;
  }
  // Production dispatches completed lines through the irrigation safety gate.
  // nullptr denotes an invalid/overlong line; no pump operation occurs here.
  bool feedLine(char c, const char*& line) {
    if (c == '\r' || c == '\n') {
      if (size_ == 0 && !overflow_) return false;
      buffer_[size_] = '\0';
      line = overflow_ ? nullptr : buffer_;
      size_ = 0;
      overflow_ = false;
      return true;
    }
    // Reject embedded control characters, including NUL, rather than truncate.
    if (c < 32 || c > 126 || size_ >= sizeof(buffer_) - 1) overflow_ = true;
    if (!overflow_) buffer_[size_++] = c;
    return false;
  }
 private:
  char buffer_[16] = {};
  size_t size_ = 0;
  bool overflow_ = false;
};
}
