#include "../esp32/mqtt_client_config.h"
#include <cassert>
#include <iostream>
using namespace plant_mqtt;
int main(){
  Config config;Topics topics;esp_mqtt_client_config_t sdk;
  assert(!clientConfig(config,topics,sdk));
  uint8_t mac[]={0x38,0x3e,0x51,0x6f,0xc7,0xa8};assert(topics.initialize(mac));
  strcpy(config.host,"broker.example.invalid");config.port=8883;
  strcpy(config.ca,"-----BEGIN CERTIFICATE-----\nfixture\n-----END CERTIFICATE-----");
  strcpy(config.username,"fixture-user");strcpy(config.password,"fixture-mqtt-password");
  if(!kTlsDateVerification){
    assert(!clientConfig(config,topics,sdk));
    std::cout<<"PASS MQTT fails closed without native date checking or public hook (shape shim)\n";
    return 0;
  }
  assert(clientConfig(config,topics,sdk));
  assert(sdk.broker.address.transport==MQTT_TRANSPORT_OVER_SSL&&!sdk.broker.address.uri);
  assert(sdk.broker.address.hostname==config.host&&sdk.broker.address.port==8883);
  assert(!sdk.broker.verification.skip_cert_common_name_check);
  if(kTlsNativeDates){assert(sdk.broker.verification.certificate==config.ca&&!sdk.broker.verification.crt_bundle_attach);}
  else {
    assert(!sdk.broker.verification.certificate&&sdk.broker.verification.crt_bundle_attach==attachMqttTrust);
    assert(prepareMqttTrust(config.ca));mbedtls_ssl_config tls;
    assert(sdk.broker.verification.crt_bundle_attach(&tls)==0);
    assert(tls.ca&&tls.authmode==MBEDTLS_SSL_VERIFY_REQUIRED&&tls.verify==MqttTrust::verify);
  }
  assert(!sdk.broker.verification.common_name); // SDK checks configured hostname
  assert(sdk.credentials.authentication.password==config.password&&sdk.credentials.username==config.username);
  assert(sdk.credentials.client_id==topics.client&&!sdk.session.disable_clean_session);
  assert(sdk.session.protocol_ver==MQTT_PROTOCOL_V_3_1_1&&sdk.session.keepalive==15);
  assert(!strcmp(sdk.session.last_will.topic,topics.status)&&!strcmp(sdk.session.last_will.msg,"{\"status\":\"OFFLINE\"}"));
  assert(sdk.session.last_will.qos==1&&sdk.session.last_will.retain==1);
  assert(sdk.network.disable_auto_reconnect&&sdk.network.timeout_ms==3000);
  assert(sdk.outbox.limit==1024&&sdk.buffer.size==512&&sdk.buffer.out_size==512);
  std::cout<<"PASS actual MQTT config builder: verified TLS hostname/CA/auth, bounded buffers, clean session, retained QoS1 LWT (SDK shape shim only)\n";
}
