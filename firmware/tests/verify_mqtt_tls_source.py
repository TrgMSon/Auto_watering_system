"""Read-only audit of pinned official sources; no downloads or SDK execution."""
import argparse
import hashlib
from pathlib import Path

IDF = 'b774170ff46'
MQTT = '0e9ec171b448db2101ea537300976b7ffe595941'
MBEDTLS = '9d669eadb1955d348986b9280156710aaaadf79f'
HASHES = {
    'esp_tls_mbedtls.c': 'd8403ebfbfcd59f2b8a064514c6fce2799fbf370ddb95deeff29eee6e59839aa',
    'transport_ssl.c': '1b9e42e556f8ef85d441950d0487eca77a8e9bde8293d7f0332f0826f08f6bde',
    'mqtt_client.c': '0651edbb34dee8bf9babc684ba4ccf231ed562fdab6218f0aa5b4d80f6a990fb',
    'x509.c': 'cdd67a9f53f3029c712cfd843038ffaf25dc477f9de215f55e585a15bc164995',
    'x509_crt.c': '26e695b0f69c353f7dd116d3a1ed5641d3c17adc0f0c6f17c317fb5099449e86',
    'ssl_tls.c': '89509f760211ad2034d6a5dd3297d997bc2a50a96aa4d4dac3cdd2c8ec3d5bd1',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_dir', type=Path)
    args = parser.parse_args()
    sources = {}
    for name, digest in HASHES.items():
        data = (args.source_dir / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == digest, f'{name}: not audited revision'
        sources[name] = data.decode('utf-8')
    mqtt = sources['mqtt_client.c']
    assert 'config->broker.verification.crt_bundle_attach' in mqtt
    assert 'esp_transport_ssl_crt_bundle_attach(ssl, cfg->crt_bundle_attach)' in mqtt
    task = mqtt[mqtt.index('static void esp_mqtt_task(void *pv)'):]
    connect = task.index('esp_transport_connect(client->transport,')
    credentials = task.index('esp_mqtt_connect(client,')
    assert connect < task.index('esp_mqtt_abort_connection(client);', connect) < credentials
    assert 'mqtt_msg_connect(&client->mqtt_state.connection,' in mqtt
    transport = sources['transport_ssl.c']
    assert 'ssl->cfg.crt_bundle_attach = crt_bundle_attach;' in transport
    assert 'esp_tls_conn_new_sync(host, strlen(host), port, &ssl->cfg, ssl->tls) <= 0' in transport
    tls = sources['esp_tls_mbedtls.c']
    setup = tls[tls.index('esp_err_t set_client_config('):]
    assert 'mbedtls_ssl_set_hostname(&tls->ssl, use_host)' in setup
    hook = setup.index('cfg->crt_bundle_attach(&tls->conf);')
    assert setup.rfind('mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);', 0, hook) >= 0
    # Return is ignored: application must install a rejecting verifier on failure.
    assert 'if (cfg->crt_bundle_attach(&tls->conf)' not in setup
    assert tls.index('set_client_config(hostname, hostlen,') < tls.index('mbedtls_ssl_setup(&tls->ssl, &tls->conf)')
    assert 'ret = mbedtls_ssl_handshake(&tls->ssl);' in tls
    x509 = sources['x509_crt.c']
    merge = x509[x509.index('static int x509_crt_merge_flags_with_cb('):]
    assert merge.index('for (i = ver_chain->len; i != 0; --i)') < merge.index('f_vrfy(p_vrfy, cur->crt, (int) i-1, &cur_flags)') < merge.index('*flags |= cur_flags;')
    assert 'return ret;' in merge[:merge.index('*flags |= cur_flags;')]
    checked = x509[x509.index('/* Check the chain */'):]
    assert checked.index('x509_crt_verify_chain(') < checked.index('x509_crt_merge_flags_with_cb(')
    assert 'if (*flags != 0)' in checked and 'return MBEDTLS_ERR_X509_CERT_VERIFY_FAILED;' in checked
    assert '#else  /* MBEDTLS_HAVE_TIME_DATE */' in sources['x509.c']
    assert 'f_vrfy = ssl->conf->f_vrfy;' in sources['ssl_tls.c']
    print(f'PASS pinned source ordering/verification audit: IDF {IDF}, MQTT {MQTT}, mbedTLS {MBEDTLS}; not TLS/broker execution')


if __name__ == '__main__':
    main()
