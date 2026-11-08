#include <Arduino.h>
#include <esp_mac.h>
#include <esp_log.h>
#include <mbedtls/x509_crt.h>
#include "../../esp32/board_config.h"
#include "protocol.h"
static mqtt_usb::Frame frame;
static plant_mqtt::Config candidate,scratch;
static uint8_t mac[6];
static bool finished=false;
void setup(){
  digitalWrite(board_config::kRelayIn1Pin,LOW);
  pinMode(board_config::kRelayIn1Pin,OUTPUT);
  esp_log_level_set("nvs",ESP_LOG_NONE);
  Serial.begin(115200);
  if(esp_read_mac(mac,ESP_MAC_WIFI_STA)!=ESP_OK){finished=true;Serial.println("ERROR identity");return;}
  Serial.printf("MQP1 DEVICE %02X:%02X:%02X:%02X:%02X:%02X\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
void loop(){
  if(finished){delay(1);return;}
  if(frame.expired(millis())){frame.clear();finished=true;Serial.println("ERROR timeout");return;}
  for(unsigned i=0;i<32&&Serial.available();++i){
    const int state=frame.feed(uint8_t(Serial.read()),millis());if(!state)continue;
    finished=true;
    if(state<0){Serial.println("ERROR frame");return;}
    auto result=mqtt_usb::Result::Invalid;
    if(mqtt_usb::decode(frame.record,frame.size(),mac,candidate))
      result=mqtt_usb::store(candidate,mac,frame.replace(),mqtt_usb::parseCertificate,scratch);
    frame.clear();ble_commands::wipeWifi(&candidate,sizeof(candidate));ble_commands::wipeWifi(&scratch,sizeof(scratch));
    Serial.println(result==mqtt_usb::Result::Verified?"OK verified":
        result==mqtt_usb::Result::Exists?"ERROR exists":result==mqtt_usb::Result::Invalid?"ERROR invalid":
        result==mqtt_usb::Result::NotWritten?"ERROR not-written":"ERROR unconfirmed");
    return;
  }
  delay(1);
}
