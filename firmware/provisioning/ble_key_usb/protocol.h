#pragma once
#include "../../esp32/ble_key_record.h"
namespace provisioning {
struct Writer : ble_keys::Storage {
  virtual bool write(const uint8_t* p, size_t n)=0;
};
enum class Result { Ok, Invalid, Exists, StorageError };
enum class RecordStatus { Missing, Valid, Invalid, Unreadable };
// Accept only the read interface: this operation has no write capability.
inline RecordStatus inspect(ble_keys::Storage& storage, const uint8_t* mac) {
  uint8_t record[ble_keys::kRecordMax]={}; size_t size=0;
  auto read=storage.read(record,sizeof(record),size);
  auto status=read==ble_keys::Read::Missing ? RecordStatus::Missing :
      read==ble_keys::Read::Error ? RecordStatus::Unreadable :
      ble_keys::valid(record,size,mac) ? RecordStatus::Valid : RecordStatus::Invalid;
  ble_keys::wipe(record,sizeof(record));
  return status;
}
inline Result store(Writer& s, const uint8_t* record, size_t n, const uint8_t* mac, bool replace) {
  if (!ble_keys::valid(record,n,mac)) return Result::Invalid;
  uint8_t previous[ble_keys::kRecordMax]={}; size_t size=0;
  auto r=s.read(previous,sizeof(previous),size);
  ble_keys::wipe(previous,sizeof(previous));
  // Unreadable/oversize existing records require explicit replacement too.
  if (r!=ble_keys::Read::Missing && !replace) return Result::Exists;
  if (!s.write(record,n)) return Result::StorageError;
  r=s.read(previous,sizeof(previous),size);
  bool ok=r==ble_keys::Read::Ok && size==n && !memcmp(record,previous,n) && ble_keys::valid(previous,size,mac);
  ble_keys::wipe(previous,sizeof(previous));
  return ok ? Result::Ok : Result::StorageError;
}
// One binary frame per boot: "BKP1", replace byte (0/1), uint16 LE record size,
// then record. Total deadline is 10 seconds from first byte (not idle timeout).
class Frame {
 public:
  ~Frame() { clear(); }
  bool expired(uint32_t now) const { return started_ && uint32_t(now-start_)>=10000; }
  int feed(uint8_t b, uint32_t now) {
    if (failed_ || done_ || expired(now)) { clear(); failed_=true; return -1; }
    if (!started_) { started_=true; start_=now; }
    if (used_<7) {
      header_[used_++]=b;
      // BKS1 is a separate, read-only request; BKP1 frames remain unchanged.
      if (used_<=4 && b!=uint8_t("BKP1"[used_-1]) && !(used_==3 && b=='S')) return fail();
      if (used_==4 && header_[2]=='S') { done_=true; return 2; }
      if (used_==5 && b>1) return fail();
      if (used_==7) {
        expected_=size_t(header_[5])+(size_t(header_[6])<<8);
        if (expected_<ble_keys::kHeader+5 || expected_>sizeof(record)) return fail();
      }
      return 0;
    }
    record[used_++-7]=b;
    if (used_==expected_+7) { done_=true; return 1; }
    return 0;
  }
  bool replace() const { return header_[4]==1; }
  size_t size() const { return expected_; }
  void clear() { ble_keys::wipe(record,sizeof(record)); ble_keys::wipe(header_,sizeof(header_)); }
  uint8_t record[ble_keys::kRecordMax]={};
 private:
  int fail() { clear(); failed_=true; return -1; }
  uint8_t header_[7]={}; size_t used_=0, expected_=0;
  uint32_t start_=0; bool started_=false, failed_=false, done_=false;
};
}
