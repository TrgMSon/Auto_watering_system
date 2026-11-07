#include "mbedtls/x509_crt.h"
namespace mqtt_tls_shim {int parseResult=0;}
void mbedtls_x509_crt_init(mbedtls_x509_crt* c){*c=mbedtls_x509_crt{};}
void mbedtls_x509_crt_free(mbedtls_x509_crt* c){*c=mbedtls_x509_crt{};}
int mbedtls_x509_crt_parse(mbedtls_x509_crt* c,const unsigned char* p,size_t n){
  c->raw.p=const_cast<unsigned char*>(p);c->raw.len=n;return mqtt_tls_shim::parseResult;
}
