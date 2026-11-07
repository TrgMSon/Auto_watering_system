#pragma once
#ifdef ARDUINO
#include "ble_pairing.h"
#include <esp_gap_ble_api.h>
namespace ble_pairing {
class EspSdk : public Sdk {
 public:
  bool setup() override {
    uint8_t auth=ESP_LE_AUTH_REQ_SC_BOND,io=ESP_IO_CAP_NONE,size=16;
    uint8_t keys=ESP_BLE_ENC_KEY_MASK|ESP_BLE_ID_KEY_MASK;
    uint8_t only=ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_ENABLE;
    return esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,&auth,sizeof(auth))==ESP_OK&&
      esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,&io,sizeof(io))==ESP_OK&&
      esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,&size,sizeof(size))==ESP_OK&&
      esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY,&keys,sizeof(keys))==ESP_OK&&
      esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY,&keys,sizeof(keys))==ESP_OK&&
      esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH,&only,sizeof(only))==ESP_OK;
  }
  bool bonded(const uint8_t* peer,bool& found) override {
    found=false;int count=esp_ble_get_bond_device_num();
    if(count<0||count>kCapacity)return false;
    if(!count)return true;
    int actual=count;
    bool ok=esp_ble_get_bond_device_list(&actual,bonds_)==ESP_OK&&actual>=0&&actual<=count;
    if(ok)for(int i=0;i<actual;++i)if(!memcmp(peer,bonds_[i].bd_addr,6))found=true;
    // API returns bond keys too: fixed static object storage, never event/log/stack.
    ble_commands::wipeWifi(bonds_,sizeof(bonds_));return ok;
  }
  bool encrypt(const uint8_t* peer) override {
    esp_bd_addr_t address;memcpy(address,peer,6);
    return esp_ble_set_encryption(address,ESP_BLE_SEC_ENCRYPT)==ESP_OK;
  }
  bool respond(const uint8_t* peer,bool accept) override {
    esp_bd_addr_t address;memcpy(address,peer,6);
    return esp_ble_gap_security_rsp(address,accept)==ESP_OK;
  }
 private:
  static constexpr int kCapacity=16;
  esp_ble_bond_dev_t bonds_[kCapacity]={};
};
}
#endif
