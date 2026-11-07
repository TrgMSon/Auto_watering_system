#pragma once
#include "ble_auth.h"
namespace ble_commands {
class EspCrypto : public Crypto {
 public:
  bool random(uint8_t (&bytes)[8]) override;
  bool hmac(const uint8_t* key, size_t keySize, const char* message, size_t messageSize,
      uint8_t (&digest)[32]) override;
};
}
