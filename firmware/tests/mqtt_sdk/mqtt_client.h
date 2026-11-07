#pragma once
#include <stdint.h>
// Shape-only SDK configuration shim. No TLS/network or MQTT implementation.
enum { MQTT_TRANSPORT_OVER_SSL=2, MQTT_PROTOCOL_V_3_1_1=4 };
struct esp_mqtt_client_config_t {
  struct {
    struct {const char* uri=nullptr;const char* hostname=nullptr;int transport=0;uint32_t port=0;} address;
    struct {const char* certificate=nullptr;bool skip_cert_common_name_check=false;const char* common_name=nullptr;int (*crt_bundle_attach)(void*)=nullptr;} verification;
  } broker;
  struct {const char* username=nullptr;const char* client_id=nullptr;struct {const char* password=nullptr;} authentication;} credentials;
  struct {
    struct {const char* topic=nullptr;const char* msg=nullptr;int qos=0,retain=0;} last_will;
    bool disable_clean_session=false;int protocol_ver=0,keepalive=0;
  } session;
  struct {bool disable_auto_reconnect=false;int timeout_ms=0;} network;
  struct {int size=0,out_size=0;} buffer;
  struct {uint64_t limit=0;} outbox;
  struct {int priority=0,stack_size=0;} task;
};
