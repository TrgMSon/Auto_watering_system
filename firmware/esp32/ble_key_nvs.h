#pragma once
#include "ble_key_record.h"
#include <nvs.h>
namespace ble_keys {
constexpr const char* kNamespace="ble_keys";
constexpr const char* kRecord="record";
class NvsReader : public Storage {
 public:
  Read read(uint8_t* out, size_t capacity, size_t& size) override {
    size=0; nvs_handle_t h;
    esp_err_t e=nvs_open(kNamespace,NVS_READONLY,&h);
    if (e==ESP_ERR_NVS_NOT_FOUND) return Read::Missing;
    if (e!=ESP_OK) return Read::Error;
    size_t n=0; e=nvs_get_blob(h,kRecord,nullptr,&n);
    if (e==ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return Read::Missing; }
    if (e!=ESP_OK || !n || n>capacity) { nvs_close(h); return Read::Error; }
    e=nvs_get_blob(h,kRecord,out,&n); nvs_close(h);
    if (e!=ESP_OK) return Read::Error;
    size=n; return Read::Ok;
  }
};
}
