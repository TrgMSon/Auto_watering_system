#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

namespace configuration {
struct Config {
  float soilStart = 30, soilStop = 70, waterMin = 20;
  float fullCm = 3.00f, emptyCm = 12.26f;
  uint32_t maxRunSeconds = 120;  // Configuration only; pump cutoff remains 5 seconds.
};
inline bool valid(const Config& c) {
  return c.maxRunSeconds >= 60 && c.maxRunSeconds <= 600 &&
      isfinite(c.soilStart) && isfinite(c.soilStop) && isfinite(c.waterMin) &&
      isfinite(c.fullCm) && isfinite(c.emptyCm) &&
      c.soilStart >= 0 && c.soilStop <= 100 && c.soilStart < c.soilStop &&
      c.waterMin >= 20 && c.waterMin <= 100 &&
      c.fullCm >= 2 && c.fullCm < c.emptyCm && c.emptyCm <= 12.26f &&
      c.emptyCm - c.waterMin / 100 * (c.emptyCm - c.fullCm) <=
          12.26f - 0.20f * (12.26f - 3.00f);
}
inline bool equal(const Config& a, const Config& b) {
  return a.soilStart == b.soilStart && a.soilStop == b.soilStop &&
      a.waterMin == b.waterMin && a.fullCm == b.fullCm && a.emptyCm == b.emptyCm &&
      a.maxRunSeconds == b.maxRunSeconds;
}
// Explicit little-endian records; schema 1 retains its exact original layout.
constexpr size_t kLegacyRecordSize = 32;
constexpr size_t kRecordSize = 36;
static_assert(sizeof(float) == 4, "Storage requires float32");
inline uint32_t checksum(const uint8_t* bytes, size_t size) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
inline void put32(uint8_t* bytes, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(value >> (8 * i));
}
inline uint32_t get32(const uint8_t* bytes) {
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[i]) << (8 * i);
  return value;
}
inline void encode(const Config& c, uint8_t* bytes) {
  put32(bytes, 0x34474643); put32(bytes + 4, 2);
  const float values[] = {c.soilStart, c.soilStop, c.waterMin, c.fullCm, c.emptyCm};
  for (unsigned i = 0; i < 5; ++i) {
    uint32_t bits; memcpy(&bits, &values[i], 4); put32(bytes + 8 + i * 4, bits);
  }
  put32(bytes + 28, c.maxRunSeconds);
  put32(bytes + 32, checksum(bytes, 32));
}
inline bool decode(const uint8_t* bytes, size_t size, Config& c) {
  if (!bytes || (size != kLegacyRecordSize && size != kRecordSize)) return false;
  const uint32_t version = get32(bytes + 4);
  if (get32(bytes) != 0x34474643 ||
      !((version == 1 && size == kLegacyRecordSize) || (version == 2 && size == kRecordSize)) ||
      get32(bytes + size - 4) != checksum(bytes, size - 4)) return false;
  float v[5];
  for (unsigned i = 0; i < 5; ++i) {
    const uint32_t bits = get32(bytes + 8 + i * 4); memcpy(&v[i], &bits, 4);
  }
  Config candidate; candidate.soilStart=v[0]; candidate.soilStop=v[1];
  candidate.waterMin=v[2]; candidate.fullCm=v[3]; candidate.emptyCm=v[4];
  if (version == 2) candidate.maxRunSeconds = get32(bytes + 28);
  if (!valid(candidate)) return false;
  c = candidate; return true;
}
class Storage {
 public:
  virtual ~Storage() {}
  virtual bool read(uint8_t* bytes, size_t capacity, size_t& actualSize) = 0;
  virtual bool write(const uint8_t* bytes, size_t size) = 0;
};
enum class Result { Saved, Unchanged, Invalid, Busy, StorageFailed };
class Manager {
 public:
  void load(Storage* storage) {
    storage_ = storage; active_ = Config{}; persistedSchema_ = 0;
    uint8_t bytes[kRecordSize]; Config candidate; size_t size = 0;
    if (storage_ && storage_->read(bytes, sizeof(bytes), size) && decode(bytes, size, candidate)) {
      active_ = candidate; persistedSchema_ = get32(bytes + 4);
    }
  }
  Result update(const Config& candidate, bool manual, bool pumpOff) {
    if (!manual || !pumpOff) return Result::Busy;
    if (!valid(candidate)) return Result::Invalid;
    if (persistedSchema_ == 2 && equal(candidate, active_)) return Result::Unchanged;
    uint8_t bytes[kRecordSize]; encode(candidate, bytes);
    if (!storage_ || !storage_->write(bytes, sizeof(bytes))) {
      persistedSchema_ = 0;  // A failed write need not leave flash unchanged.
      return Result::StorageFailed;
    }
    uint8_t verified[kRecordSize]; size_t size = 0; Config decoded;
    if (!storage_->read(verified, sizeof(verified), size) || size != sizeof(bytes) ||
        memcmp(bytes, verified, sizeof(bytes)) != 0 || !decode(verified, size, decoded)) {
      // Flash may have changed: do not skip a later explicit repair save.
      persistedSchema_ = 0;
      return Result::StorageFailed;
    }
    active_ = decoded; persistedSchema_ = 2; return Result::Saved;
  }
  const Config& active() const { return active_; }
  bool persisted() const { return persistedSchema_ != 0; }
  uint32_t persistedSchema() const { return persistedSchema_; }
 private:
  Config active_;
  Storage* storage_ = nullptr;
  uint32_t persistedSchema_ = 0;
};
inline bool parseSet(const char* text, const Config& active, Config& candidate) {
  if (!text || strncmp(text, "SET ", 4)) return false;
  const char* keys[] = {"SOIL_START ", "SOIL_STOP ", "WATER_MIN ", "FULL_CM ", "EMPTY_CM "};
  candidate = active;
  float* fields[] = {&candidate.soilStart, &candidate.soilStop, &candidate.waterMin,
                     &candidate.fullCm, &candidate.emptyCm};
  for (unsigned i = 0; i < 5; ++i) {
    size_t n = strlen(keys[i]);
    if (strncmp(text + 4, keys[i], n)) continue;
    const char* value = text + 4 + n;
    // Decimal only; no whitespace, hex floats, NaN, infinity, or suffixes.
    const char* p = value;
    if (*p == '+' || *p == '-') ++p;
    bool digits = false;
    while (*p >= '0' && *p <= '9') { digits = true; ++p; }
    if (*p == '.') { ++p; while (*p >= '0' && *p <= '9') { digits = true; ++p; } }
    if (!digits) return false;
    if (*p == 'e' || *p == 'E') {
      ++p; if (*p == '+' || *p == '-') ++p;
      const char* start = p;
      while (*p >= '0' && *p <= '9') ++p;
      if (p == start) return false;
    }
    if (*p) return false;
    errno = 0; char* end;
    *fields[i] = strtof(value, &end);
    return errno != ERANGE && *end == 0 && valid(candidate);
  }
  return false;
}
}  // namespace configuration
