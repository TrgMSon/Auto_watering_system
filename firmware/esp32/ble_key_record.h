#pragma once
#include "ble_auth.h"
#include <string.h>
namespace ble_keys {
constexpr size_t kHeader = 11, kRecordMax = kHeader + ble_commands::kKeyMax + 4;
inline void wipe(void* data, size_t size) {
  volatile uint8_t* p = static_cast<volatile uint8_t*>(data);
  while (size--) *p++ = 0;
}
inline uint32_t checksum(const uint8_t* p, size_t n) {
  uint32_t c = 0xffffffff;
  while (n--) { c ^= *p++; for (unsigned i=0;i<8;++i) c = (c>>1) ^ (0xedb88320u & (0u-(c&1))); }
  return ~c;
}
inline bool valid(const uint8_t* p, size_t n, const uint8_t* mac) {
  if (n < kHeader+5 || n > kRecordMax || p[0]!='B' || p[1]!='K' || p[2]!=1 ||
      memcmp(p+3,mac,6) || p[10]!=0 || !p[9] || p[9]>ble_commands::kKeyMax ||
      n != kHeader+p[9]+4) return false;
  uint32_t c=checksum(p,n-4);
  for (unsigned i=0;i<4;++i) if (p[n-4+i] != uint8_t(c>>(8*i))) return false;
  return true;
}
inline size_t encode(uint8_t* p, const uint8_t* mac, const uint8_t* key, size_t n) {
  if (!n || n>ble_commands::kKeyMax) return 0;
  p[0]='B'; p[1]='K'; p[2]=1; memcpy(p+3,mac,6); p[9]=uint8_t(n); p[10]=0;
  memcpy(p+kHeader,key,n); uint32_t c=checksum(p,kHeader+n);
  for (unsigned i=0;i<4;++i) p[kHeader+n+i]=uint8_t(c>>(8*i));
  return kHeader+n+4;
}
enum class Read { Ok, Missing, Error };
struct Storage {
  virtual ~Storage() {}
  virtual Read read(uint8_t* out, size_t capacity, size_t& size)=0;
};
enum class Status { Loaded, Missing, Invalid };
class KeySource : public ble_commands::KeySource {
 public:
  ~KeySource() { wipe(key_,sizeof(key_)); }
  Status initialize(Storage& storage, const uint8_t* mac) {
    wipe(key_,sizeof(key_)); size_=0;
    uint8_t record[kRecordMax]={}; size_t n=0;
    Read r=storage.read(record,sizeof(record),n);
    Status s=r==Read::Missing ? Status::Missing : Status::Invalid;
    if (r==Read::Ok && valid(record,n,mac)) {
      size_=record[9]; memcpy(key_,record+kHeader,size_); s=Status::Loaded;
    }
    wipe(record,sizeof(record)); return s;
  }
  bool load(uint8_t* out, size_t capacity, size_t& size) const override {
    size=0; if (!size_ || capacity<size_) return false;
    memcpy(out,key_,size_); size=size_; return true;
  }
 private:
  uint8_t key_[ble_commands::kKeyMax]={}; size_t size_=0;
};
}
