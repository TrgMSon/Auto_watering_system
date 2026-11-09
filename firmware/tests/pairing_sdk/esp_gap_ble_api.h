#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
using esp_bd_addr_t=uint8_t[6];
struct esp_ble_bond_dev_t{uint8_t bd_addr[6],keyFixture[64];};
constexpr int ESP_OK=0,ESP_LE_AUTH_REQ_SC_BOND=9,ESP_IO_CAP_NONE=3,ESP_BLE_ENC_KEY_MASK=1,ESP_BLE_ID_KEY_MASK=2,ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE=1;
constexpr int ESP_BLE_SM_AUTHEN_REQ_MODE=0,ESP_BLE_SM_IOCAP_MODE=1,ESP_BLE_SM_MAX_KEY_SIZE=2,ESP_BLE_SM_SET_INIT_KEY=3,ESP_BLE_SM_SET_RSP_KEY=4,ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH=5,ESP_BLE_SEC_ENCRYPT=1;
int esp_ble_gap_set_security_param(int,void*,uint8_t);
int esp_ble_get_bond_device_num();int esp_ble_get_bond_device_list(int*,esp_ble_bond_dev_t*);
int esp_ble_set_encryption(esp_bd_addr_t,int);int esp_ble_gap_security_rsp(esp_bd_addr_t,bool);
