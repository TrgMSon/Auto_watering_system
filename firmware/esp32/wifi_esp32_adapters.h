#pragma once
#ifdef ARDUINO
#include "wifi_radio.h"
#include "wifi_credentials.h"
#include <esp_event.h>
#include <esp_netif.h>
#include <atomic>
namespace ble_wifi {
class EspRadioSdk : public RadioSdk {
 public:
  ~EspRadioSdk();
  EspRadioSdk()=default;
  EspRadioSdk(const EspRadioSdk&)=delete;
  EspRadioSdk& operator=(const EspRadioSdk&)=delete;
  bool initialize() override;
  bool stop() override;
  bool barrier(uint32_t token) override;
  bool drain(uint32_t& token) override;
  bool clearIp() override;
  bool configureRam(const ble_commands::WifiRequest& candidate) override;
  bool startConnect() override;
  bool candidateIp(const ble_commands::WifiRequest& candidate) override;
  uint32_t ipv4() const override{return ipv4_;}
 private:
  static void event(void*,esp_event_base_t,int32_t,void*);
  bool cleanup();
  esp_netif_t* netif_=nullptr;
  esp_event_handler_instance_t wifiHandler_=nullptr,ipHandler_=nullptr,barrierHandler_=nullptr;
  bool wifiOwned_=false,ready_=false,failed_=false;
  uint32_t ipv4_=0;
  bool candidateReported_=false;
  std::atomic<uint32_t> count_{0},ack_{0};std::atomic<bool> fault_{false};
  // Diagnostic snapshots only; never used for connection/security decisions.
  // Repeated events between worker polls are coalesced; no credential copies.
  std::atomic<uint32_t> diagnosticEvents_{0},disconnectReason_{0};
};
class NvsCredentialBlob : public Blob {
 public:
  Read read(uint8_t*,size_t,size_t&) override;
  Write writeCommit(const uint8_t*,size_t) override;
};
}
#endif
