#include "mqtt_tls.h"
#include <time.h>
namespace plant_mqtt {
bool mqttUtc(mbedtls_x509_time& out){
  const time_t epoch=time(nullptr);
  if(epoch<ble_commands::kUnixMin||epoch>ble_commands::kUnixMax)return false;
  tm utc={};
#ifdef _WIN32
  if(gmtime_s(&utc,&epoch)!=0)return false;
#else
  if(!gmtime_r(&epoch,&utc))return false;
#endif
  out.year=utc.tm_year+1900;out.mon=utc.tm_mon+1;out.day=utc.tm_mday;
  out.hour=utc.tm_hour;out.min=utc.tm_min;out.sec=utc.tm_sec;return true;
}
namespace {MqttTrust trust;}
bool prepareMqttTrust(const char* pem){return kTlsDateHook&&trust.prepare(pem);}
int attachMqttTrust(void* config){return trust.attach(config);}
}
