#pragma once
#include <stdint.h>
#include <stddef.h>
using esp_err_t=int;using nvs_handle_t=int;
constexpr int ESP_OK=0,ESP_ERR_NVS_NOT_FOUND=1,ESP_ERR_NVS_TYPE_MISMATCH=2,ESP_ERR_NVS_INVALID_LENGTH=3;
constexpr int NVS_READONLY=0,NVS_READWRITE=1;
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_get_u32(nvs_handle_t,const char*,uint32_t*);
esp_err_t nvs_get_u16(nvs_handle_t,const char*,uint16_t*);
esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
esp_err_t nvs_get_str(nvs_handle_t,const char*,char*,size_t*);
esp_err_t nvs_set_u32(nvs_handle_t,const char*,uint32_t);
esp_err_t nvs_set_u16(nvs_handle_t,const char*,uint16_t);
esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
esp_err_t nvs_set_str(nvs_handle_t,const char*,const char*);
esp_err_t nvs_commit(nvs_handle_t);
