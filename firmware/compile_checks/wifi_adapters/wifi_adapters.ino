#include <Arduino.h>
#include "../../esp32/wifi_esp32_adapters.h"
// Constructors only: no init, connect, NVS read/write or activation path.
static ble_wifi::EspRadioSdk sdk;
static ble_wifi::EspRadio radio(sdk);
static ble_wifi::NvsCredentialBlob blob;
static const uint8_t unusedIdentity[6]={};
static ble_wifi::CredentialStorage storage(blob,unusedIdentity);
// Volatile references retain the real adapter virtual tables and implementations.
ble_wifi::RadioSdk* volatile linkedSdk=&sdk;
ble_wifi::Radio* volatile linkedRadio=&radio;
ble_wifi::Blob* volatile linkedBlob=&blob;
ble_wifi::Storage* volatile linkedStorage=&storage;
using Service=void(ble_wifi::Attempt::*)(uint32_t,uint64_t,ble_wifi::Radio&,ble_wifi::Storage&,const ble_commands::WifiSecurity&);
Service volatile linkedAttemptService=&ble_wifi::Attempt::service;
void setup() {}
void loop() {
  auto a=linkedSdk;auto b=linkedRadio;auto c=linkedBlob;auto d=linkedStorage;auto e=linkedAttemptService;
  (void)a;(void)b;(void)c;(void)d;(void)e;
  delay(1);
}
