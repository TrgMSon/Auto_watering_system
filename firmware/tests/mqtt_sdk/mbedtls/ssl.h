#pragma once
#include "x509_crt.h"
#include "build_info.h"
constexpr int MBEDTLS_SSL_VERIFY_REQUIRED=2;
struct mbedtls_ssl_config {
  int authmode=0;mbedtls_x509_crt* ca=nullptr;void* context=nullptr;
  int (*verify)(void*,mbedtls_x509_crt*,int,uint32_t*)=nullptr;
};
inline void mbedtls_ssl_conf_authmode(mbedtls_ssl_config* c,int mode){c->authmode=mode;}
inline void mbedtls_ssl_conf_ca_chain(mbedtls_ssl_config* c,mbedtls_x509_crt* ca,void*){c->ca=ca;}
inline void mbedtls_ssl_conf_verify(mbedtls_ssl_config* c,int (*f)(void*,mbedtls_x509_crt*,int,uint32_t*),void* p){c->verify=f;c->context=p;}
