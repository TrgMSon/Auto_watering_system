#include "ble_auth.h"
#include "ble_clock.h"
#include <stdio.h>
#include <string.h>
namespace ble_commands {
namespace {
void wipe(void* data, size_t size) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(data);
  while (size--) *p++ = 0;
}
class Parser {
 public:
  Parser(const char* p, size_t n) : p_(p), end_(p + n) {}
  void space() { while (p_ != end_ && (*p_ == ' ' || *p_ == '\r' || *p_ == '\n' || *p_ == '\t')) ++p_; }
  bool take(char c) { space(); if (p_ == end_ || *p_ != c) return false; ++p_; return true; }
  bool done() { space(); return p_ == end_; }
  bool string(char* out, size_t capacity) {
    if (!take('"')) return false;
    size_t n = 0;
    while (p_ != end_ && *p_ != '"') {
      const unsigned char c = static_cast<unsigned char>(*p_++);
      // This fixed ASCII command schema has no escaped-string alternative.
      if (c < 32 || c > 126 || c == '\\' || n + 1 >= capacity) return false;
      out[n++] = static_cast<char>(c);
    }
    if (p_ == end_) return false;
    ++p_; out[n] = 0; return true;
  }
  bool integer(uint32_t& value, bool& range) {
    space(); bool negative = false;
    if (p_ != end_ && *p_ == '-') { negative = true; ++p_; }
    if (p_ == end_ || *p_ < '0' || *p_ > '9') return false;
    const bool zero = *p_ == '0'; unsigned digits = 0; value = 0;
    while (p_ != end_ && *p_ >= '0' && *p_ <= '9') {
      unsigned digit = *p_++ - '0'; ++digits;
      if (value > (UINT32_MAX - digit) / 10) range = true;
      else if (!range) value = value * 10 + digit;
    }
    if (zero && digits != 1) return false;
    if (negative) range = true;
    return true;  // Decimal/exponent suffix is rejected by the next delimiter.
  }
  bool tenths(uint32_t& value, bool& range) {
    uint32_t whole = 0;
    if (!integer(whole, range)) return false;
    uint32_t fraction = 0;
    // A decimal point must immediately follow the integer; no whitespace inside numbers.
    if (p_ != end_ && *p_ == '.') {
      ++p_;
      if (p_ == end_ || *p_ < '0' || *p_ > '9') return false;
      fraction = *p_++ - '0';
      if (p_ != end_ && *p_ >= '0' && *p_ <= '9') return false;
    }
    if (whole > (UINT32_MAX - fraction) / 10) range = true;
    value = range ? 0 : whole * 10 + fraction;
    return true;
  }
 private:
  const char* p_; const char* end_;
};
}
const char* name(Op op) { return op == Op::On ? "PUMP_ON" : op == Op::Off ? "PUMP_OFF" :
    op == Op::Auto ? "AUTO" : op == Op::Manual ? "MANUAL" : op == Op::Time ? "TIME" :
    op == Op::Config ? "CONFIG" : nullptr; }
const char* name(Code code) {
  switch (code) {
    case Code::Ok: return "OK"; case Code::BadSig: return "BAD_SIG";
    case Code::NonceExpired: return "NONCE_EXPIRED"; case Code::BadJson: return "BAD_JSON";
    case Code::OutOfRange: return "OUT_OF_RANGE"; case Code::LowWater: return "LOW_WATER";
    case Code::Maintenance: return "MAINTENANCE"; case Code::SensorError: return "SENSOR_ERROR";
    case Code::PumpBusy: return "PUMP_BUSY"; case Code::InvalidTime: return "INVALID_TIME";
    case Code::StorageError: return "STORAGE_ERROR";
    case Code::OtaInProgress: return "OTA_IN_PROGRESS";
    case Code::WifiOk: return "WIFI_OK"; case Code::WifiFail: return "WIFI_FAIL";
    default: return nullptr;
  }
}
Code parse(const char* bytes, size_t size, Command& command, Op endpoint) {
  command = Command{};
  const bool time = endpoint == Op::Time;
  if (time) command.op = Op::Time;
  if (!bytes || !size || size > kPayloadMax) return Code::BadJson;
  Parser p(bytes, size); bool cmd = false, dur = false, sig = false, range = false, badSig = false;
  if (!p.take('{')) return Code::BadJson;
  for (unsigned fields = 0; fields < (time ? 2u : 3u); ++fields) {
    char key[8];
    if (!p.string(key, sizeof(key)) || !p.take(':')) return Code::BadJson;
    if (!time && !strcmp(key, "cmd")) {
      if (cmd) { command.op = Op::Unknown; return Code::BadJson; }
      char op[16]; if (!p.string(op, sizeof(op))) return Code::BadJson;
      cmd = true; command.op = !strcmp(op, "PUMP_ON") ? Op::On : !strcmp(op, "PUMP_OFF") ? Op::Off :
          !strcmp(op, "AUTO") ? Op::Auto : !strcmp(op, "MANUAL") ? Op::Manual : Op::Unknown;
    } else if (time && !strcmp(key, "unix")) {
      if (dur || !p.integer(command.unixSeconds, range)) return Code::BadJson;
      dur = true;
    } else if (!time && !strcmp(key, "dur")) {
      if (dur || !p.integer(command.seconds, range)) return Code::BadJson;
      dur = true;
    } else if (!strcmp(key, "sig")) {
      // A string of incorrect length is BAD_SIG; wrong JSON type is BAD_JSON.
      char value[kPayloadMax + 1];
      if (sig || !p.string(value, sizeof(value))) return Code::BadJson;
      sig = true; badSig = strlen(value) != 16;
      for (size_t i = 0; value[i]; ++i) if (!((value[i] >= '0' && value[i] <= '9') ||
          (value[i] >= 'a' && value[i] <= 'f'))) badSig = true;
      if (!badSig) memcpy(command.signature, value, 17);
    } else return Code::BadJson;
    if (p.take('}')) {
      if (!p.done() || (!time && !cmd) || !dur || !sig || command.op == Op::Unknown) return Code::BadJson;
      if (time) return range || !validUnix(command.unixSeconds) ? Code::InvalidTime :
          badSig ? Code::BadSig : Code::Ok;
      if (range || ((command.op == Op::Off || command.op == Op::Auto || command.op == Op::Manual) ?
          command.seconds != 0 : command.seconds < 1 || command.seconds > 600))
        return Code::OutOfRange;
      return badSig ? Code::BadSig : Code::Ok;
    }
    if (!p.take(',')) return Code::BadJson;
  }
  return Code::BadJson;
}
Code validateConfig(const ConfigRequest& r) {
  return r.minPercentTenths < r.maxPercentTenths && r.maxPercentTenths <= 1000 &&
      r.minWaterPercentTenths >= 200 && r.minWaterPercentTenths <= 1000 &&
      r.autoMode <= 1 && r.maxDurationSeconds >= 60 && r.maxDurationSeconds <= 600 ?
      Code::Ok : Code::OutOfRange;
}
Code parseConfig(const char* bytes, size_t size, ConfigRequest& request) {
  request = ConfigRequest{};
  if (!bytes || !size || size > kConfigPayloadMax) return Code::BadJson;
  ConfigRequest r;
  Parser p(bytes, size); unsigned seen = 0; bool range = false, badSig = false;
  if (!p.take('{')) return Code::BadJson;
  for (unsigned field = 0; field < 6; ++field) {
    char key[10]; if (!p.string(key, sizeof(key)) || !p.take(':')) return Code::BadJson;
    unsigned bit = 0;
    if (!strcmp(key, "min")) bit = 1;
    else if (!strcmp(key, "max")) bit = 2;
    else if (!strcmp(key, "auto")) bit = 4;
    else if (!strcmp(key, "max_dur")) bit = 8;
    else if (!strcmp(key, "min_water")) bit = 16;
    else if (!strcmp(key, "sig")) bit = 32;
    if (!bit || (seen & bit)) return Code::BadJson;
    seen |= bit;
    if (bit == 32) {
      char signature[kConfigPayloadMax + 1];
      if (!p.string(signature, sizeof(signature))) return Code::BadJson;
      badSig = strlen(signature) != 16;
      for (size_t i=0; signature[i]; ++i) if (!((signature[i]>='0' && signature[i]<='9') ||
          (signature[i]>='a' && signature[i]<='f'))) badSig = true;
      if (!badSig) memcpy(r.signature, signature, 17);
    } else {
      uint32_t* value = bit == 1 ? &r.minPercentTenths : bit == 2 ? &r.maxPercentTenths :
          bit == 4 ? &r.autoMode : bit == 8 ? &r.maxDurationSeconds : &r.minWaterPercentTenths;
      if (!(bit == 4 || bit == 8 ? p.integer(*value, range) : p.tenths(*value, range))) return Code::BadJson;
    }
    if (p.take('}')) {
      if (seen != 63 || !p.done()) return Code::BadJson;
      if (range || validateConfig(r) != Code::Ok) return Code::OutOfRange;
      if (badSig) return Code::BadSig;
      request = r; return Code::Ok;
    }
    if (!p.take(',')) return Code::BadJson;
  }
  return Code::BadJson;
}
size_t configCanonical(const char* nonce, const ConfigRequest& r, char* out, size_t capacity) {
  if (!out || !capacity) return 0;
  out[0] = 0;
  if (!nonce || validateConfig(r) != Code::Ok) return 0;
  for (unsigned i=0; i<16; ++i) if (!((nonce[i]>='0' && nonce[i]<='9') ||
      (nonce[i]>='a' && nonce[i]<='f'))) return 0;
  if (nonce[16] != 0) return 0;
  const int n = snprintf(out, capacity, "%s|CONFIG|%lu.%lu|%lu.%lu|%lu|%lu|%lu.%lu", nonce,
      static_cast<unsigned long>(r.minPercentTenths/10), static_cast<unsigned long>(r.minPercentTenths%10),
      static_cast<unsigned long>(r.maxPercentTenths/10), static_cast<unsigned long>(r.maxPercentTenths%10),
      static_cast<unsigned long>(r.autoMode), static_cast<unsigned long>(r.maxDurationSeconds),
      static_cast<unsigned long>(r.minWaterPercentTenths/10), static_cast<unsigned long>(r.minWaterPercentTenths%10));
  if (n <= 0 || size_t(n) >= capacity) { out[0] = 0; return 0; }
  return size_t(n);
}
Authenticator::~Authenticator() { wipe(key_, sizeof(key_)); reset(); }
void Authenticator::loadKey(const KeySource* source) {
  wipe(key_, sizeof(key_)); keySize_ = 0;
  size_t size = 0;
  if (source && source->load(key_, sizeof(key_), size) && size && size <= sizeof(key_)) keySize_ = size;
  if (!keySize_) wipe(key_, sizeof(key_));
}
void Authenticator::reset() { wipe(nonce_, sizeof(nonce_)); valid_ = pinned_ = false; issuedMs_ = 0; }
bool Authenticator::readNonce(uint16_t offset, uint32_t now, const char*& bytes, uint16_t& size) {
  bytes = nullptr; size = 0;
  if (!offset) {
    reset(); uint8_t random[8];
    if (!crypto_.random(random)) return false;
    const char hex[] = "0123456789abcdef";
    for (unsigned i = 0; i < 8; ++i) { nonce_[i*2] = hex[random[i] >> 4]; nonce_[i*2+1] = hex[random[i] & 15]; }
    wipe(random, sizeof(random)); issuedMs_ = now; valid_ = pinned_ = true;
  }
  if (!pinned_ || offset > 16) return false;
  bytes = nonce_ + offset; size = 16 - offset; return true;
}
Authentication Authenticator::authenticate(const char* bytes, size_t size, uint32_t now, Op endpoint) {
  Authentication out;
  const bool usable = valid_ && static_cast<uint32_t>(now - issuedMs_) < kNonceTtlMs;
  consume();  // Before parser, key, signature or range rejection.
  out.code = parse(bytes, size, out.command, endpoint);
  if (!keySize_) { out.missingKey = true; return out; }
  if (!usable) { out.code = Code::NonceExpired; return out; }
  if (out.code != Code::Ok) return out;
  char canonical[48];
  const int n = snprintf(canonical, sizeof(canonical), "%s|%s|%lu", nonce_, name(out.command.op),
      static_cast<unsigned long>(out.command.op == Op::Time ? out.command.unixSeconds : out.command.seconds));
  if (n < 0 || size_t(n) >= sizeof(canonical)) { out.missingKey = true; return out; }
  out.code = verifySignature(canonical, size_t(n), out.command.signature, out.missingKey);
  wipe(canonical, sizeof(canonical)); return out;
}
ConfigAuthentication Authenticator::authenticateConfig(const char* bytes, size_t size, uint32_t now) {
  ConfigAuthentication out;
  const bool usable = valid_ && static_cast<uint32_t>(now - issuedMs_) < kNonceTtlMs;
  consume();
  out.code = parseConfig(bytes, size, out.request);
  if (!keySize_) { out.missingKey = true; return out; }
  if (!usable) { out.code = Code::NonceExpired; return out; }
  if (out.code != Code::Ok) return out;
  char canonical[kConfigCanonicalCapacity];
  const size_t n = configCanonical(nonce_, out.request, canonical, sizeof(canonical));
  if (!n) { out.missingKey = true; return out; }
  out.code = verifySignature(canonical, n, out.request.signature, out.missingKey);
  wipe(canonical, sizeof(canonical)); return out;
}
Code Authenticator::verifySignature(const char* canonical, size_t size, const char* signature, bool& cryptoFailure) {
  uint8_t digest[32] = {};
  if (!crypto_.hmac(key_, keySize_, canonical, size, digest)) {
    wipe(digest, sizeof(digest)); cryptoFailure = true; return Code::None;
  }
  const char hex[] = "0123456789abcdef"; volatile uint8_t difference = 0;
  for (unsigned i = 0; i < 16; ++i) {
    const char expected = hex[(i & 1) ? digest[i/2] & 15 : digest[i/2] >> 4];
    difference = static_cast<uint8_t>(difference | static_cast<uint8_t>(expected ^ signature[i]));
  }
  wipe(digest, sizeof(digest));
  return difference ? Code::BadSig : Code::Ok;
}
size_t resultJson(Op op, Code code, char (&out)[kResultCapacity]) {
  out[0] = 0;
  if (!name(op) || !name(code)) return 0;
  const int n = snprintf(out, sizeof(out), "{\"op\":\"%s\",\"code\":\"%s\"}", name(op), name(code));
  return n > 0 && size_t(n) < sizeof(out) ? size_t(n) : 0;
}
}
