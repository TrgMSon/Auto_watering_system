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
};
inline bool valid(const Config& c) {
  return isfinite(c.soilStart) && isfinite(c.soilStop) && isfinite(c.waterMin) &&
      isfinite(c.fullCm) && isfinite(c.emptyCm) &&
      c.soilStart >= 0 && c.soilStop <= 100 && c.soilStart < c.soilStop &&
      c.waterMin >= 20 && c.waterMin <= 100 &&
      c.fullCm >= 2 && c.fullCm < c.emptyCm && c.emptyCm <= 12.26f &&
      c.emptyCm - c.waterMin / 100 * (c.emptyCm - c.fullCm) <=
          12.26f - 0.20f * (12.26f - 3.00f);
}
inline bool equal(const Config& a, const Config& b) {
  return a.soilStart == b.soilStart && a.soilStop == b.soilStop &&
      a.waterMin == b.waterMin && a.fullCm == b.fullCm && a.emptyCm == b.emptyCm;
}
// Single 32-byte blob: magic, schema, five IEEE float32 values, FNV-1a checksum.
constexpr size_t kRecordSize = 32;
static_assert(sizeof(float) == 4, "Storage requires float32");
inline uint32_t checksum(const uint8_t* bytes) {
  uint32_t hash = 2166136261u;
  for (size_t i = 0; i < 28; ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
inline void encode(const Config& c, uint8_t* bytes) {
  const uint32_t magic = 0x34474643, version = 1;
  memcpy(bytes, &magic, 4); memcpy(bytes + 4, &version, 4);
  const float values[] = {c.soilStart, c.soilStop, c.waterMin, c.fullCm, c.emptyCm};
  memcpy(bytes + 8, values, 20);
  const uint32_t hash = checksum(bytes); memcpy(bytes + 28, &hash, 4);
}
inline bool decode(const uint8_t* bytes, Config& c) {
  uint32_t magic, version, hash;
  memcpy(&magic, bytes, 4); memcpy(&version, bytes + 4, 4); memcpy(&hash, bytes + 28, 4);
  if (magic != 0x34474643 || version != 1 || hash != checksum(bytes)) return false;
  float v[5]; memcpy(v, bytes + 8, 20);
  Config candidate; candidate.soilStart=v[0]; candidate.soilStop=v[1];
  candidate.waterMin=v[2]; candidate.fullCm=v[3]; candidate.emptyCm=v[4];
  if (!valid(candidate)) return false;
  c = candidate; return true;
}
class Storage {
 public:
  virtual ~Storage() {}
  virtual bool read(uint8_t* bytes, size_t size) = 0;
  virtual bool write(const uint8_t* bytes, size_t size) = 0;
};
enum class Result { Saved, Unchanged, Invalid, Busy, StorageFailed };
class Manager {
 public:
  void load(Storage* storage) {
    storage_ = storage; active_ = Config{}; persisted_ = false;
    uint8_t bytes[kRecordSize]; Config candidate;
    if (storage_ && storage_->read(bytes, sizeof(bytes)) && decode(bytes, candidate)) {
      active_ = candidate; persisted_ = true;
    }
  }
  Result update(const Config& candidate, bool manual, bool pumpOff) {
    if (!manual || !pumpOff) return Result::Busy;
    if (!valid(candidate)) return Result::Invalid;
    if (persisted_ && equal(candidate, active_)) return Result::Unchanged;
    uint8_t bytes[kRecordSize]; encode(candidate, bytes);
    if (!storage_ || !storage_->write(bytes, sizeof(bytes))) return Result::StorageFailed;
    active_ = candidate; persisted_ = true; return Result::Saved;
  }
  const Config& active() const { return active_; }
  bool persisted() const { return persisted_; }
 private:
  Config active_;
  Storage* storage_ = nullptr;
  bool persisted_ = false;
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
