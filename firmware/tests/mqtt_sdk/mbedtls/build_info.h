#pragma once
#define MBEDTLS_X509_CRT_PARSE_C
#define MBEDTLS_SSL_CLI_C
#ifndef MQTT_TEST_NO_DATE
#define MBEDTLS_HAVE_TIME_DATE
#endif
