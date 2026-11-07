#include "ble_crypto.h"
#ifdef ARDUINO
#include "esp_random.h"
#include "mbedtls/md.h"
namespace ble_commands {
bool EspCrypto::random(uint8_t (&bytes)[8]) {
  // Invoked only after Bluetooth is running, providing the RNG entropy source.
  esp_fill_random(bytes, sizeof(bytes)); return true;
}
bool EspCrypto::hmac(const uint8_t* key, size_t keySize, const char* message,
    size_t messageSize, uint8_t (&digest)[32]) {
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  return info && mbedtls_md_hmac(info, key, keySize,
      reinterpret_cast<const unsigned char*>(message), messageSize, digest) == 0;
}
}
#endif
