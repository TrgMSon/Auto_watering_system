#pragma once
#include "mqtt_telemetry.h"
#include <mqtt_client.h>
#include "mqtt_tls.h"
namespace plant_mqtt {
// Caller keeps Config and Topics alive for the entire SDK client lifetime.
inline bool clientConfig(const Config& config,const Topics& topics,esp_mqtt_client_config_t& c){
  c={};if(!kTlsDateVerification||!configured(config)||!topics.identity[0])return false;
  c.broker.address.hostname=config.host;c.broker.address.port=config.port;
  c.broker.address.transport=MQTT_TRANSPORT_OVER_SSL;
  if(kTlsNativeDates)c.broker.verification.certificate=config.ca;
  else c.broker.verification.crt_bundle_attach=attachMqttTrust;
  c.broker.verification.skip_cert_common_name_check=false;
  c.credentials.client_id=topics.client;c.credentials.username=config.username;
  c.credentials.authentication.password=config.password;
  c.session.protocol_ver=MQTT_PROTOCOL_V_3_1_1;c.session.keepalive=15;
  c.session.last_will.topic=topics.status;c.session.last_will.msg=kOffline;
  c.session.last_will.qos=1;c.session.last_will.retain=1;
  c.network.disable_auto_reconnect=true;c.network.timeout_ms=3000;
  c.buffer.size=512;c.buffer.out_size=512;c.outbox.limit=1024;
  c.task.priority=1;c.task.stack_size=6144;
  return true;
}
}
