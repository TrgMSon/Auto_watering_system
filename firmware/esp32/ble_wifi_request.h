#pragma once
#include <stdint.h>
#include <stddef.h>
namespace ble_commands {
inline void wipeWifi(void* bytes, size_t n) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(bytes);
  while (n--) *p++ = 0;
}
// Initial policy: personal WPA2 PSK, 8..63 UTF-8 passphrase bytes (AP compatibility must be verified).
// SSID: 1..32 valid UTF-8 bytes. No enterprise, open, raw-hex PSK or WPA3 promise.
struct WifiRequest {
  char ssid[33] = {}, password[64] = {}, signature[17] = {};
  ~WifiRequest() { clear(); }
  void clear() { wipeWifi(ssid,sizeof(ssid)); wipeWifi(password,sizeof(password)); wipeWifi(signature,sizeof(signature)); }
};
bool validWifiRequest(const WifiRequest& request);
// Worst-case escaped strings exceed MTU185. Parser budget is independent of ATT.
constexpr size_t kWifiPayloadMax = 640, kWifiCanonicalCapacity = 119;
// Current bond/authentication state. Per-WRITE proof never upgrades connection encryption.
class WifiSecurity {
 public:
  void connect(uint64_t generation) { generation_=generation; paired_=bonded_=encrypted_=false; }
  void update(uint64_t generation, bool paired, bool bonded, bool encrypted) {
    if (!generation || generation!=generation_) return;
    paired_=paired; bonded_=bonded; encrypted_=encrypted;
  }
  void disconnect(uint64_t generation) { if (generation==generation_) connect(0); }
  bool permits(uint64_t generation) const {
    return generation && generation==generation_ && paired_ && bonded_ && encrypted_;
  }
  bool bondedPeer(uint64_t generation) const { return generation&&generation==generation_&&paired_&&bonded_; }
  bool permitsWrite(uint64_t generation,uint64_t encryptedWriteGeneration) const {
    return bondedPeer(generation)&&(encrypted_||encryptedWriteGeneration==generation);
  }
 private:
  uint64_t generation_=0;
  bool paired_=false, bonded_=false, encrypted_=false;
};
}
