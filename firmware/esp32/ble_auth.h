#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ble_wifi_request.h"
namespace ble_commands {
constexpr size_t kPayloadMax = 128, kKeyMax = 128;
constexpr uint32_t kNonceTtlMs = 30000;
enum class Op { Unknown, On, Off, Time, Config };
// CONFIG uses the existing authenticated Write With Response policy.
constexpr bool authenticatedWrite(unsigned suffix) { return suffix == 2 || suffix == 3 || suffix == 7; }
constexpr bool encryptedWifiWrite(unsigned suffix) { return suffix == 6; }
enum class Code { None, Ok, BadSig, NonceExpired, BadJson, OutOfRange,
  LowWater, Maintenance, SensorError, PumpBusy, InvalidTime, WifiOk, WifiFail, StorageError, OtaInProgress };
const char* name(Op op);
const char* name(Code code);
struct Crypto {
  virtual ~Crypto() {}
  virtual bool random(uint8_t (&bytes)[8]) = 0;
  virtual bool hmac(const uint8_t* key, size_t keySize, const char* message,
      size_t messageSize, uint8_t (&digest)[32]) = 0;
};
struct KeySource {
  virtual ~KeySource() {}
  // UTF-8 bytes of the whole secret string, never Base64-decoded. Boot-only.
  virtual bool load(uint8_t* out, size_t capacity, size_t& size) const = 0;
};
// Exact tenths of operational calibrated percent; not volume percent.
struct ConfigRequest {
  uint32_t minPercentTenths = 0, maxPercentTenths = 0;
  uint32_t autoMode = 0, maxDurationSeconds = 0, minWaterPercentTenths = 0;
  char signature[17] = {};
};
struct Command { Op op = Op::Unknown; uint32_t seconds = 0, unixSeconds = 0; ConfigRequest config; char signature[17] = {}; };
struct Authentication {
  Command command;
  Code code = Code::None;
  bool missingKey = false;
};
Code parse(const char* bytes, size_t size, Command& command, Op endpoint = Op::Unknown);
// Compact maximum is 90 bytes; allow bounded whitespace, independently of pump/TIME.
constexpr size_t kConfigPayloadMax = 192, kConfigCanonicalCapacity = 64;
// One ATT Write Request only. MTU 185 supports at most 182 bytes of CONFIG.
constexpr bool configWriteFits(size_t size, uint16_t mtu, bool prepared = false,
    bool withResponse = true, uint16_t offset = 0) {
  return !prepared && withResponse && !offset && size && size <= kConfigPayloadMax &&
      mtu >= 3 && size <= size_t(mtu - 3);
}
Code validateConfig(const ConfigRequest& request);
Code parseConfig(const char* bytes, size_t size, ConfigRequest& request);
// Returns 0 on invalid request/nonce or insufficient output space. No truncation.
size_t configCanonical(const char* nonce, const ConfigRequest& request, char* out, size_t capacity);
struct ConfigAuthentication {
  ConfigRequest request;
  Code code = Code::None; // Ok means authenticated/validated only, never applied.
  bool missingKey = false;
};
Code parseWifi(const char* bytes, size_t size, WifiRequest& request);
size_t wifiCanonical(const char* nonce, const WifiRequest& request, char* out, size_t capacity);
struct WifiAuthentication {
  WifiRequest request;
  uint64_t generation = 0, encryptedWriteGeneration = 0;
  Code code = Code::None;
  bool missingKey = false, securityDenied = false, transportDenied = false;
};
class Authenticator {
 public:
  explicit Authenticator(Crypto& crypto) : crypto_(crypto) {}
  ~Authenticator();
  void loadKey(const KeySource* source);
  bool hasKey() const { return keySize_ != 0; }
  void reset();
  bool readNonce(uint16_t offset, uint32_t now, const char*& bytes, uint16_t& size);
  Authentication authenticate(const char* bytes, size_t size, uint32_t now, Op endpoint = Op::Unknown);
  ConfigAuthentication authenticateConfig(const char* bytes, size_t size, uint32_t now);
  // Gate before dereferencing payload. A future callback must also gate BEFORE copy.
  WifiAuthentication authenticateWifi(const char* bytes, size_t size, uint32_t now,
      const WifiSecurity& security, uint64_t generation, uint16_t mtu,
      bool prepared = false, bool withResponse = true, uint16_t offset = 0,
      uint64_t encryptedWriteGeneration = 0);
  // Every authenticated WRITE consumes the current nonce, even transport-level rejects.
  void consume() { valid_ = false; }
 private:
  Code verifySignature(const char* canonical, size_t size, const char* signature, bool& cryptoFailure);
  Crypto& crypto_;
  uint8_t key_[kKeyMax] = {};
  size_t keySize_ = 0;
  char nonce_[17] = {};
  uint32_t issuedMs_ = 0;
  bool valid_ = false, pinned_ = false;
};
constexpr size_t kResultCapacity = 64;
constexpr size_t kMaxResultBytes = sizeof("{\"op\":\"PUMP_OFF\",\"code\":\"NONCE_EXPIRED\"}") - 1;
size_t resultJson(Op op, Code code, char (&out)[kResultCapacity]);
}
