#include "ota_service.h"

#ifdef ARDUINO
#include "ble_wifi_service.h"
#include "firmware_version.h"
#include "ota_config.h"
#include "pump.h"

#include <Arduino.h>
#include <atomic>
#include <cJSON.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>

namespace plant_ota {
namespace {
constexpr uint32_t kCheckIntervalMs = 60UL * 60UL * 1000UL;
constexpr uint32_t kRetryIntervalMs = 5UL * 60UL * 1000UL;
constexpr size_t kCheckResponseBytes = 2048;
constexpr size_t kDownloadChunkBytes = 1024;

// Mbed TLS 3 renamed the SHA-256 *_ret APIs while preserving their int return
// values. Keep the OTA digest checks compatible with both Mbed TLS 2 and 3.
int sha256Starts(mbedtls_sha256_context* context, int is224) {
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  return mbedtls_sha256_starts(context, is224);
#else
  return mbedtls_sha256_starts_ret(context, is224);
#endif
}

int sha256Update(mbedtls_sha256_context* context, const unsigned char* input, size_t size) {
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  return mbedtls_sha256_update(context, input, size);
#else
  return mbedtls_sha256_update_ret(context, input, size);
#endif
}

int sha256Finish(mbedtls_sha256_context* context, unsigned char* output) {
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  return mbedtls_sha256_finish(context, output);
#else
  return mbedtls_sha256_finish_ret(context, output);
#endif
}

std::atomic<bool> requested{false};
std::atomic<bool> approved{false};
std::atomic<bool> taskRunning{false};
std::atomic<uint32_t> nextCheck{0};
bool started = false;
bool timeSyncRequested = false;

struct Release {
  char version[65]{};
  char sha256[65]{};
  char downloadPath[128]{};
  size_t sizeBytes = 0;
};

class HttpSession {
 public:
  bool open(const char* url) {
    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 15000;
    config.buffer_size = 1024;
    config.buffer_size_tx = 512;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    client_ = esp_http_client_init(&config);
    if (!client_) return false;
    if (esp_http_client_open(client_, 0) != ESP_OK) return false;
    contentLength_ = esp_http_client_fetch_headers(client_);
    statusCode_ = esp_http_client_get_status_code(client_);
    return true;
  }

  ~HttpSession() {
    if (client_) esp_http_client_cleanup(client_);
  }

  esp_http_client_handle_t get() const { return client_; }
  int statusCode() const { return statusCode_; }
  int64_t contentLength() const { return contentLength_; }

 private:
  esp_http_client_handle_t client_ = nullptr;
  int statusCode_ = 0;
  int64_t contentLength_ = -1;
};

bool validVersion(const char* value) {
  if (!value || !value[0]) return false;
  size_t length = 0;
  for (const char* p = value; *p; ++p) {
    const char c = *p;
    const bool alphaNumeric = (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alphaNumeric || (length != 0 && (c == '.' || c == '_' || c == '+' || c == '-')))) return false;
    if (++length > 64) return false;
  }
  return true;
}

bool validSha256(const char* value) {
  if (!value || strlen(value) != 64) return false;
  for (const char* p = value; *p; ++p) {
    const char c = *p;
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

bool readResponse(HttpSession& session, char* output, size_t capacity, size_t& length) {
  length = 0;
  if (!output || capacity < 2) return false;
  while (length < capacity - 1) {
    const int count = esp_http_client_read(session.get(), output + length, capacity - 1 - length);
    if (count < 0) return false;
    if (count == 0) break;
    length += static_cast<size_t>(count);
  }
  output[length] = '\0';
  if (length == capacity - 1) {
    char extra;
    if (esp_http_client_read(session.get(), &extra, 1) != 0) return false;
  }
  return esp_http_client_is_complete_data_received(session.get());
}

bool parseRelease(const char* body, size_t length, Release& release, bool& updateAvailable) {
  cJSON* root = cJSON_ParseWithLength(body, length);
  if (!root) return false;
  const cJSON* success = cJSON_GetObjectItemCaseSensitive(root, "success");
  const cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
  bool valid = cJSON_IsTrue(success) && cJSON_IsObject(data);
  const cJSON* update = valid
      ? cJSON_GetObjectItemCaseSensitive(data, "update_available")
      : nullptr;
  valid = valid && cJSON_IsBool(update);
  updateAvailable = valid && cJSON_IsTrue(update);
  if (valid && updateAvailable) {
    const cJSON* version = cJSON_GetObjectItemCaseSensitive(data, "latest_version");
    const cJSON* sha256 = cJSON_GetObjectItemCaseSensitive(data, "sha256");
    const cJSON* size = cJSON_GetObjectItemCaseSensitive(data, "size_bytes");
    const cJSON* path = cJSON_GetObjectItemCaseSensitive(data, "download_url");
    valid = cJSON_IsString(version) && cJSON_IsString(sha256) &&
        cJSON_IsNumber(size) && cJSON_IsString(path) &&
        validVersion(version->valuestring) && validSha256(sha256->valuestring) &&
        size->valuedouble >= 1 && size->valuedouble <= 4 * 1024 * 1024 &&
        size->valuedouble == static_cast<double>(static_cast<size_t>(size->valuedouble));
    if (valid) {
      const int versionLength = snprintf(release.version, sizeof(release.version), "%s", version->valuestring);
      const int shaLength = snprintf(release.sha256, sizeof(release.sha256), "%s", sha256->valuestring);
      const int pathLength = snprintf(release.downloadPath, sizeof(release.downloadPath), "%s", path->valuestring);
      valid = versionLength > 0 && static_cast<size_t>(versionLength) < sizeof(release.version) &&
          shaLength == 64 && pathLength > 0 && static_cast<size_t>(pathLength) < sizeof(release.downloadPath) &&
          static_cast<size_t>(size->valuedouble) <= 4 * 1024 * 1024;
      if (valid) {
        release.sizeBytes = static_cast<size_t>(size->valuedouble);
        char expectedPath[96];
        const int expectedLength = snprintf(expectedPath, sizeof(expectedPath),
            "/api/firmware/%s/download", release.version);
        valid = expectedLength > 0 && static_cast<size_t>(expectedLength) < sizeof(expectedPath) &&
            strcmp(release.downloadPath, expectedPath) == 0;
      }
    }
  }
  cJSON_Delete(root);
  return valid;
}

bool makeCheckUrl(char* output, size_t capacity) {
  const int baseLength = snprintf(output, capacity, "%s/api/firmware/check?currentVersion=",
      kOtaApiBaseUrl);
  if (baseLength < 0 || static_cast<size_t>(baseLength) >= capacity) return false;
  size_t used = static_cast<size_t>(baseLength);
  for (const char* p = kFirmwareVersion; *p; ++p) {
    const char* encoded = *p == '+' ? "%2B" : nullptr;
    const size_t count = encoded ? 3 : 1;
    if (used + count >= capacity) return false;
    if (encoded) {
      memcpy(output + used, encoded, count);
      used += count;
    } else {
      output[used++] = *p;
    }
  }
  output[used] = '\0';
  return true;
}

bool checkForUpdate(Release& release, bool& updateAvailable) {
  char url[256];
  if (!makeCheckUrl(url, sizeof(url))) {
    Serial.println("[OTA] check URL is too long");
    return false;
  }
  HttpSession session;
  if (!session.open(url)) {
    Serial.println("[OTA] HTTPS version check failed");
    return false;
  }
  if (session.statusCode() != 200 || session.contentLength() > kCheckResponseBytes) {
    Serial.printf("[OTA] version check returned HTTP %d\n", session.statusCode());
    return false;
  }
  char body[kCheckResponseBytes + 1];
  size_t length = 0;
  if (!readResponse(session, body, sizeof(body), length)) {
    Serial.println("[OTA] invalid or oversized version-check response");
    return false;
  }
  if (!parseRelease(body, length, release, updateAvailable)) {
    Serial.println("[OTA] malformed version-check response");
    return false;
  }
  if (updateAvailable && strcmp(release.version, kFirmwareVersion) == 0) {
    Serial.println("[OTA] backend returned the current version; update skipped");
    updateAvailable = false;
  }
  return true;
}

bool hashMatches(const unsigned char digest[32], const char* expected) {
  static const char hex[] = "0123456789abcdef";
  char actual[65];
  for (size_t i = 0; i < 32; ++i) {
    actual[i * 2] = hex[digest[i] >> 4];
    actual[i * 2 + 1] = hex[digest[i] & 0x0f];
  }
  actual[64] = '\0';
  return strcasecmp(actual, expected) == 0;
}

bool installRelease(const Release& release) {
  const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
  if (!partition || release.sizeBytes > partition->size) {
    Serial.println("[OTA] no OTA slot or firmware exceeds the inactive partition");
    return false;
  }

  char url[256];
  const int urlLength = snprintf(url, sizeof(url), "%s%s", kOtaApiBaseUrl, release.downloadPath);
  if (urlLength <= 0 || static_cast<size_t>(urlLength) >= sizeof(url)) {
    Serial.println("[OTA] firmware download URL is too long");
    return false;
  }
  HttpSession session;
  if (!session.open(url)) {
    Serial.println("[OTA] HTTPS firmware download failed");
    return false;
  }
  if (session.statusCode() != 200 ||
      (session.contentLength() >= 0 &&
       static_cast<uint64_t>(session.contentLength()) != release.sizeBytes)) {
    Serial.printf("[OTA] firmware download returned HTTP %d or invalid length\n", session.statusCode());
    return false;
  }

  esp_ota_handle_t handle;
  if (esp_ota_begin(partition, release.sizeBytes, &handle) != ESP_OK) {
    Serial.println("[OTA] could not begin writing the inactive partition");
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  bool ok = sha256Starts(&sha, 0) == 0;
  unsigned char buffer[kDownloadChunkBytes];
  size_t received = 0;
  bool firstByte = true;
  while (ok && received < release.sizeBytes) {
    const size_t remaining = release.sizeBytes - received;
    const int count = esp_http_client_read(session.get(), reinterpret_cast<char*>(buffer),
        remaining < sizeof(buffer) ? remaining : sizeof(buffer));
    if (count <= 0) {
      ok = false;
      break;
    }
    if (firstByte && buffer[0] != 0xE9) {
      ok = false;
      break;
    }
    firstByte = false;
    ok = sha256Update(&sha, buffer, static_cast<size_t>(count)) == 0 &&
        esp_ota_write(handle, buffer, static_cast<size_t>(count)) == ESP_OK;
    received += static_cast<size_t>(count);
  }

  unsigned char digest[32];
  if (ok) ok = received == release.sizeBytes &&
      esp_http_client_is_complete_data_received(session.get()) &&
      sha256Finish(&sha, digest) == 0 && hashMatches(digest, release.sha256);
  mbedtls_sha256_free(&sha);
  if (!ok) {
    esp_ota_abort(handle);
    Serial.println("[OTA] download, image header, or SHA-256 verification failed");
    return false;
  }
  if (esp_ota_end(handle) != ESP_OK) {
    Serial.println("[OTA] ESP32 rejected the firmware image");
    return false;
  }
  if (esp_ota_set_boot_partition(partition) != ESP_OK) {
    Serial.println("[OTA] could not select the new firmware partition");
    return false;
  }
  Serial.printf("[OTA] verified and installed version %s; restarting\n", release.version);
  delay(100);
  esp_restart();
  return true;
}

bool clockReady() {
  const time_t now = time(nullptr);
  return now >= 1704067200 && now <= 4102444800;
}

void finishWorker(uint32_t delayMs) {
  nextCheck.store(millis() + delayMs);
  taskRunning.store(false);
  vTaskDelete(nullptr);
}

void worker(void*) {
  Release release;
  bool updateAvailable = false;
  if (!checkForUpdate(release, updateAvailable)) {
    finishWorker(kRetryIntervalMs);
    return;
  }
  if (!updateAvailable) {
    finishWorker(kCheckIntervalMs);
    return;
  }

  const esp_partition_t* partition = esp_ota_get_next_update_partition(nullptr);
  if (!partition || release.sizeBytes > partition->size) {
    Serial.println("[OTA] no OTA slot or firmware exceeds the inactive partition");
    finishWorker(kRetryIntervalMs);
    return;
  }

  Serial.printf("[OTA] version %s available; waiting for pump OFF\n", release.version);
  requested.store(true);
  while (!approved.load()) vTaskDelay(pdMS_TO_TICKS(50));
  Serial.println("[OTA] pump commands are locked during firmware installation");
  if (!installRelease(release)) {
    requested.store(false);
    while (approved.load()) vTaskDelay(pdMS_TO_TICKS(50));
    finishWorker(kRetryIntervalMs);
  }
  requested.store(false);
  while (approved.load()) vTaskDelay(pdMS_TO_TICKS(50));
  finishWorker(kRetryIntervalMs);
}
}

bool begin() {
  if (started) return true;
  started = true;
  nextCheck.store(millis());
  Serial.println("[OTA] automatic HTTPS updater enabled; checking hourly while Wi-Fi is online");
  return true;
}

void confirmBoot() {
#if defined(CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE) && CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
  const esp_partition_t* running = esp_ota_get_running_partition();
  esp_ota_img_states_t state;
  if (!running || esp_ota_get_state_partition(running, &state) != ESP_OK ||
      state != ESP_OTA_IMG_PENDING_VERIFY) return;
  if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
    Serial.println("[OTA] new firmware boot confirmed");
    return;
  }
  Serial.println("[OTA] boot confirmation failed; requesting rollback");
  if (esp_ota_mark_app_invalid_rollback_and_reboot() != ESP_OK) {
    Serial.println("[OTA] rollback request failed; restarting current image");
    esp_restart();
  }
#endif
}

void ownerService(irrigation::System& system) {
  if (approved.load() && !requested.load()) {
    system.setOtaLock(false);
    approved.store(false);
    Serial.println("[OTA] installation ended; pump control restored");
  } else if (requested.load() && !approved.load() && !pump::isOn()) {
    system.setOtaLock(true);
    approved.store(true);
    Serial.println("[OTA] pump is OFF; firmware installation admitted");
  }
  if (!started || !ble_wifi_service::networkSnapshot().online) return;
  if (!clockReady()) {
    if (!timeSyncRequested) {
      configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
      timeSyncRequested = true;
      Serial.println("[OTA] SNTP time sync requested for HTTPS verification");
    }
    return;
  }
  if (static_cast<int32_t>(millis() - nextCheck.load()) < 0) return;
  bool expected = false;
  if (!taskRunning.compare_exchange_strong(expected, true)) return;
  const size_t freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  Serial.printf("[OTA] starting check internal_free=%luB largest_block=%luB\n",
      static_cast<unsigned long>(freeHeap), static_cast<unsigned long>(largestBlock));
  if (xTaskCreate(worker, "firmware-ota", 8192, nullptr, 1, nullptr) != pdPASS) {
    taskRunning.store(false);
    nextCheck.store(millis() + kRetryIntervalMs);
    Serial.println("[OTA] worker task could not be created");
  }
}
}
#else
namespace plant_ota {
bool begin() { return false; }
void ownerService(irrigation::System&) {}
void confirmBoot() {}
}
#endif
