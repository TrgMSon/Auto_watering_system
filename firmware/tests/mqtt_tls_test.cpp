#include "../esp32/mqtt_tls.h"
#include <cassert>
#include <iostream>
namespace mqtt_tls_shim {extern int parseResult;}
using namespace plant_mqtt;
mbedtls_x509_time now;bool clockOk=true;
bool utc(mbedtls_x509_time& out){out=now;return clockOk;}
mbedtls_x509_time date(int y,int m,int d,int h=0,int minute=0,int second=0){
  mbedtls_x509_time t;t.year=y;t.mon=m;t.day=d;t.hour=h;t.min=minute;t.sec=second;return t;
}
int main(){
  now=date(2026,11,7,12);MqttTrust trust(utc);mbedtls_ssl_config tls;
  assert(trust.prepare("fixture CA"));assert(!trust.attach(&tls));
  assert(tls.authmode==MBEDTLS_SSL_VERIFY_REQUIRED&&tls.ca&&tls.verify);
  unsigned char raw=1;mbedtls_x509_crt cert;cert.raw.p=&raw;cert.raw.len=1;
  cert.valid_from=date(2026,11,7,11);cert.valid_to=date(2026,11,7,13);
  // mbedTLS invokes the same callback for every element of the selected chain.
  for(int depth:{0,1,2}){
    now=date(2026,11,7,12);
    uint32_t flags=0;assert(!tls.verify(tls.context,&cert,depth,&flags)&&!flags);
    now=cert.valid_from;assert(!tls.verify(tls.context,&cert,depth,&flags)&&!flags);
    now=cert.valid_to;assert(!tls.verify(tls.context,&cert,depth,&flags)&&!flags);
    now=date(2026,11,7,10,59,59);flags=0;
    assert(!tls.verify(tls.context,&cert,depth,&flags)&&(flags&MBEDTLS_X509_BADCERT_FUTURE));
    now=date(2026,11,7,13,0,1);flags=0;
    assert(!tls.verify(tls.context,&cert,depth,&flags)&&(flags&MBEDTLS_X509_BADCERT_EXPIRED));
  }
  now=date(2026,11,7,12);
  // Wrong CA (NOT_TRUSTED) and hostname mismatch flags may never be cleared.
  for(uint32_t original:{8u,4u,12u}){
    uint32_t flags=original;assert(!tls.verify(tls.context,&cert,0,&flags)&&flags==original);
    now=date(2026,11,7,14);assert(!tls.verify(tls.context,&cert,1,&flags));
    assert((flags&original)==original&&(flags&MBEDTLS_X509_BADCERT_EXPIRED));now=date(2026,11,7,12);
  }
  uint32_t flags=0;clockOk=false;assert(tls.verify(tls.context,&cert,0,&flags)!=0);clockOk=true;
  for(auto bad:{date(1970,1,1),date(2100,1,1),date(2026,2,29),date(2026,11,7,24)}){
    now=bad;assert(tls.verify(tls.context,&cert,0,&flags)!=0);
  }
  now=date(2026,11,7,12);
  assert(tls.verify(nullptr,&cert,0,&flags)!=0&&tls.verify(tls.context,nullptr,0,&flags)!=0);
  assert(tls.verify(tls.context,&cert,0,nullptr)!=0);
  assert(trust.attach(nullptr)!=0&&tls.verify(tls.context,&cert,-1,&flags)!=0);
  MqttTrust noClock(nullptr);assert(noClock.prepare("fixture CA"));
  assert(MqttTrust::verify(&noClock,&cert,0,&flags)!=0);
  cert.raw.p=nullptr;assert(tls.verify(tls.context,&cert,0,&flags)!=0);cert.raw.p=&raw;
  cert.valid_to=date(2026,1,1);assert(tls.verify(tls.context,&cert,1,&flags)!=0);
  for(int parseResult:{-1,1}){
    mqtt_tls_shim::parseResult=parseResult;MqttTrust bad(utc);mbedtls_ssl_config rejected;
    assert(!bad.prepare("partial/invalid CA"));assert(bad.attach(&rejected)!=0);
    assert(rejected.verify&&rejected.authmode==MBEDTLS_SSL_VERIFY_REQUIRED&&!rejected.ca);
    assert(rejected.verify(rejected.context,&cert,0,&flags)!=0);
  }
  std::cout<<"PASS actual date callback: inclusive UTC boundaries, leaf/intermediate/root, preserved CA/hostname flags, clock/cert/parse failures (shim, no TLS handshake)\n";
}
