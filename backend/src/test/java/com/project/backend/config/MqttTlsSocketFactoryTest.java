package com.project.backend.config;

import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import org.junit.jupiter.api.Test;

class MqttTlsSocketFactoryTest {
    @Test
    void trustAllManagerAcceptsCertificatesWithoutChainValidation() {
        var trustManager = MqttTlsSocketFactory.createTrustAllManager();

        assertDoesNotThrow(() -> trustManager.checkServerTrusted(null, "RSA"));
        assertDoesNotThrow(() -> trustManager.checkClientTrusted(null, "RSA"));
        assertNotNull(MqttTlsSocketFactory.createTrustAll());
    }
}
