#pragma once
#include "autonomous_storage.h"
#ifdef ARDUINO
#include <nvs.h>
namespace autonomous_storage {
// One blob only; no dependency on configuration or desired-mode keys.
class NvsStorage : public Storage {
 public:
  Read read(uint8_t* out,size_t capacity,size_t& size) override {
    size=0;nvs_handle_t h;esp_err_t e=nvs_open("watering_auto",NVS_READONLY,&h);
    if(e==ESP_ERR_NVS_NOT_FOUND)return Read::Missing;
    if(e!=ESP_OK)return Read::Error;
    size_t n=0;e=nvs_get_blob(h,"state",nullptr,&n);
    if(e==ESP_ERR_NVS_NOT_FOUND){nvs_close(h);return Read::Missing;}
    if(e!=ESP_OK||!n||n>capacity){nvs_close(h);return Read::Error;}
    e=nvs_get_blob(h,"state",out,&n);nvs_close(h);
    if(e!=ESP_OK)return Read::Error;
    size=n;return Read::Ok;
  }
  bool writeCommit(const uint8_t* bytes,size_t size) override {
    nvs_handle_t h;if(nvs_open("watering_auto",NVS_READWRITE,&h)!=ESP_OK)return false;
    const bool ok=nvs_set_blob(h,"state",bytes,size)==ESP_OK&&nvs_commit(h)==ESP_OK;
    nvs_close(h);return ok;
  }
};
}
#endif
