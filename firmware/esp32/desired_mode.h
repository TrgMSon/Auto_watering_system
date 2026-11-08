#pragma once
#include <stdint.h>
namespace desired_mode {
// Independent typed NVS key, separate from the unchanged configuration blob.
class Storage {
 public:
  virtual ~Storage() {}
  virtual bool readMode(uint8_t& value) = 0;
  virtual bool writeMode(uint8_t value) = 0;
};
class Manager {
 public:
  void load(Storage* storage) {
    storage_=storage;value_=0;confirmed_=false;
    uint8_t value=0xff;
    if(storage_&&storage_->readMode(value)&&value<=1){value_=value;confirmed_=true;}
  }
  bool save(bool automatic) {
    const uint8_t value=automatic?1:0;
    if(confirmed_&&value_==value)return true;
    // A failed write/readback can change flash. Later explicit repair must not
    // be skipped based on the previously confirmed value. No automatic retry.
    confirmed_=false;uint8_t verified=0xff;
    if(!storage_||!storage_->writeMode(value)||!storage_->readMode(verified)||verified!=value)return false;
    value_=value;confirmed_=true;return true;
  }
  bool confirmed()const{return confirmed_;}
  bool automatic()const{return confirmed_&&value_==1;}
 private:
  Storage* storage_=nullptr;uint8_t value_=0;bool confirmed_=false;
};
}
