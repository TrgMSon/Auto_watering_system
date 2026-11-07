#pragma once
#include <stddef.h>
#include <stdint.h>
constexpr int MBEDTLS_ERR_X509_CERT_VERIFY_FAILED=-0x2700;
constexpr uint32_t MBEDTLS_X509_BADCERT_EXPIRED=1,MBEDTLS_X509_BADCERT_FUTURE=0x200;
struct mbedtls_x509_time {int year=0,mon=0,day=0,hour=0,min=0,sec=0;};
struct mbedtls_x509_crt {
  struct {unsigned char* p=nullptr;size_t len=0;} raw;
  mbedtls_x509_time valid_from,valid_to;
};
void mbedtls_x509_crt_init(mbedtls_x509_crt*);
void mbedtls_x509_crt_free(mbedtls_x509_crt*);
int mbedtls_x509_crt_parse(mbedtls_x509_crt*,const unsigned char*,size_t);
