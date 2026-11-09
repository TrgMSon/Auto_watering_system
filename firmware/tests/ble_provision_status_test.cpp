#include "../provisioning/ble_key_usb/protocol.h"
#include <cassert>
#include <vector>
#include <iostream>

// Deliberately has no write method, just like the sketch's status reader.
struct Reader : ble_keys::Storage {
  std::vector<uint8_t> bytes;
  ble_keys::Read result=ble_keys::Read::Missing;
  unsigned reads=0;
  ble_keys::Read read(uint8_t* out,size_t cap,size_t& size) override {
    ++reads; size=0;
    if(result!=ble_keys::Read::Ok) return result;
    if(bytes.size()>cap) return ble_keys::Read::Error;
    size=bytes.size(); memcpy(out,bytes.data(),size); return result;
  }
};
int main() {
  using provisioning::RecordStatus;
  uint8_t mac[]={2,3,4,5,6,7}, wrong[]={2,3,4,5,6,8};
  uint8_t record[ble_keys::kRecordMax]={}, key[43]={};
  auto size=ble_keys::encode(record,mac,key,sizeof(key));
  Reader reader;
  assert(provisioning::inspect(reader,mac)==RecordStatus::Missing);
  reader.result=ble_keys::Read::Error;
  assert(provisioning::inspect(reader,mac)==RecordStatus::Unreadable);
  reader.result=ble_keys::Read::Ok;
  reader.bytes.assign(record,record+size);
  const auto original=reader.bytes;
  assert(provisioning::inspect(reader,mac)==RecordStatus::Valid);
  assert(provisioning::inspect(reader,wrong)==RecordStatus::Invalid);
  assert(reader.bytes==original);
  reader.bytes.back()^=1;
  assert(provisioning::inspect(reader,mac)==RecordStatus::Invalid);
  assert(reader.reads==5);

  provisioning::Frame status;
  assert(status.feed('B',0)==0 && status.feed('K',1)==0);
  assert(status.feed('S',2)==0 && status.feed('1',3)==2);
  assert(status.feed(0,4)==-1); // A status request cannot become provisioning.
  for(bool replace : {false,true}) {
    provisioning::Frame frame;
    std::vector<uint8_t> bytes={'B','K','P','1',uint8_t(replace),uint8_t(size),0};
    bytes.insert(bytes.end(),record,record+size);
    int result=0;
    for(auto byte:bytes) result=frame.feed(byte,10);
    assert(result==1 && frame.replace()==replace && frame.size()==size);
  }
  provisioning::Frame invalid, timeout;
  assert(invalid.feed('B',0)==0 && invalid.feed('K',0)==0);
  assert(invalid.feed('S',0)==0 && invalid.feed('2',0)==-1);
  assert(timeout.feed('B',0)==0 && timeout.feed('K',10000)==-1);
  std::cout << "BLE provisioning read-only status PASS\n";
}
