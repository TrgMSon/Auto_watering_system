#pragma once
#include "ble_clock.h"
#include <sdkconfig.h>
#include <mbedtls/build_info.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <string.h>
namespace plant_mqtt {
#ifdef MBEDTLS_HAVE_TIME_DATE
constexpr bool kTlsNativeDates=true;
#else
constexpr bool kTlsNativeDates=false;
#endif
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) && CONFIG_MBEDTLS_CERTIFICATE_BUNDLE && defined(MBEDTLS_X509_CRT_PARSE_C) && defined(MBEDTLS_SSL_CLI_C)
constexpr bool kTlsDateHook=true;
#else
constexpr bool kTlsDateHook=false;
#endif
constexpr bool kTlsDateVerification=kTlsNativeDates||kTlsDateHook;
bool mqttUtc(mbedtls_x509_time& out);
// One immutable trust store per MQTT worker. Prepare before starting the SDK
// task; never free/change the store while a TLS connection may reference it.
class MqttTrust {
 public:
  explicit MqttTrust(bool (*utc)(mbedtls_x509_time&)=mqttUtc):utc_(utc){mbedtls_x509_crt_init(&ca_);}
  ~MqttTrust(){mbedtls_x509_crt_free(&ca_);}
  MqttTrust(const MqttTrust&)=delete;
  MqttTrust& operator=(const MqttTrust&)=delete;
  bool prepare(const char* pem){
    if(ready_)return true; // production calls once with its validated config
    if(!pem||!pem[0])return false;
    const int result=mbedtls_x509_crt_parse(&ca_,reinterpret_cast<const unsigned char*>(pem),strlen(pem)+1);
    ready_=result==0&&ca_.raw.p&&ca_.raw.len;
    if(!ready_){mbedtls_x509_crt_free(&ca_);mbedtls_x509_crt_init(&ca_);}
    return ready_;
  }
  int attach(void* config){
    if(!config)return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    auto* conf=static_cast<mbedtls_ssl_config*>(config);
    mbedtls_ssl_conf_authmode(conf,MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(conf,ready_?&ca_:nullptr,nullptr);
    // ESP-TLS ignores the attach return value. Always install a fail-closed
    // verifier, including when preparation failed; do not rely on that return.
    mbedtls_ssl_conf_verify(conf,verify,this);
    return ready_?0:MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
  }
  static int verify(void* context,mbedtls_x509_crt* certificate,int depth,uint32_t* flags){
    auto* self=static_cast<MqttTrust*>(context);mbedtls_x509_time now={};
    if(!self||!self->ready_||!certificate||!certificate->raw.p||!certificate->raw.len||!flags||depth<0||
        !self->utc_||!self->utc_(now)||now.year<2024||now.year>2099||!valid(now)||!valid(certificate->valid_from)||!valid(certificate->valid_to)||
        compare(certificate->valid_from,certificate->valid_to)>0)return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;
    // Add flags only. Never clear native chain/signature/hostname errors.
    if(compare(now,certificate->valid_from)<0)*flags|=MBEDTLS_X509_BADCERT_FUTURE;
    if(compare(now,certificate->valid_to)>0)*flags|=MBEDTLS_X509_BADCERT_EXPIRED;
    return 0; // VERIFY_REQUIRED rejects nonzero merged flags during handshake
  }
 private:
  static bool valid(const mbedtls_x509_time& t){
    if(t.year<1||t.year>9999||t.mon<1||t.mon>12||t.day<1||t.hour<0||t.hour>23||t.min<0||t.min>59||t.sec<0||t.sec>59)return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=t.year%4==0&&(t.year%100!=0||t.year%400==0);
    return t.day<=days[t.mon-1]+(t.mon==2&&leap?1:0);
  }
  static int compare(const mbedtls_x509_time& a,const mbedtls_x509_time& b){
    const int left[]={a.year,a.mon,a.day,a.hour,a.min,a.sec},right[]={b.year,b.mon,b.day,b.hour,b.min,b.sec};
    for(unsigned i=0;i<6;++i){if(left[i]<right[i])return -1;if(left[i]>right[i])return 1;}
    return 0;
  }
  mbedtls_x509_crt ca_;bool ready_=false;bool (*utc_)(mbedtls_x509_time&);
};
bool prepareMqttTrust(const char* pem);
int attachMqttTrust(void* config);
}
