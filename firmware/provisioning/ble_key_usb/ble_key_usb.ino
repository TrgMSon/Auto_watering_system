#include <Arduino.h>
#include <esp_mac.h>
#include "../../esp32/board_config.h"
#include "../../esp32/ble_key_nvs.h"
#include "protocol.h"
class NvsWriter : public provisioning::Writer {
 public:
  ble_keys::Read read(uint8_t* p, size_t cap, size_t& n) override {
    ble_keys::NvsReader reader; return reader.read(p,cap,n);
  }
  bool write(const uint8_t* p, size_t n) override {
    nvs_handle_t h;
    if (nvs_open(ble_keys::kNamespace,NVS_READWRITE,&h)!=ESP_OK) return false;
    bool ok=nvs_set_blob(h,ble_keys::kRecord,p,n)==ESP_OK && nvs_commit(h)==ESP_OK;
    nvs_close(h); return ok;
  }
};
static provisioning::Frame frame;
static uint8_t mac[6];
static bool finished=false;
void setup() {
  digitalWrite(board_config::kRelayIn1Pin,LOW);
  pinMode(board_config::kRelayIn1Pin,OUTPUT);
  Serial.begin(115200);
  if (esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK) {
    finished=true; Serial.println("ERROR identity"); return;
  }
  Serial.printf("BKP1 DEVICE %02X:%02X:%02X:%02X:%02X:%02X\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
void loop() {
  if (finished) { delay(1); return; }
  if (frame.expired(millis())) {
    frame.clear(); finished=true; Serial.println("ERROR timeout"); return;
  }
  for (unsigned i=0;i<32 && Serial.available();++i) {
    int state=frame.feed(uint8_t(Serial.read()),millis());
    if (!state) continue;
    finished=true;
    if (state<0) { Serial.println("ERROR frame"); return; }
    if (state==2) {
      ble_keys::NvsReader reader;
      auto status=provisioning::inspect(reader,mac);
      frame.clear();
      Serial.println(status==provisioning::RecordStatus::Missing ? "STATUS missing" :
          status==provisioning::RecordStatus::Valid ? "STATUS valid" :
          status==provisioning::RecordStatus::Invalid ? "STATUS invalid" : "STATUS unreadable");
      return;
    }
    NvsWriter storage;
    auto result=provisioning::store(storage,frame.record,frame.size(),mac,frame.replace());
    frame.clear();
    Serial.println(result==provisioning::Result::Ok ? "OK verified" :
        result==provisioning::Result::Exists ? "ERROR exists" :
        result==provisioning::Result::Invalid ? "ERROR invalid" : "ERROR storage");
    return;
  }
  delay(1);
}
