package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import java.util.Map;

import org.junit.jupiter.api.Test;
import org.mockito.ArgumentCaptor;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.config.MqttGateway;

class MqttDeviceCommandPublisherTest {
    private static final String DEVICE_ID = "AA:BB:CC:DD:EE:FF";

    @Test
    void publishesConfigurationAsRetainedLatestState() throws Exception {
        MqttGateway mqttGateway = org.mockito.Mockito.mock(MqttGateway.class);
        MqttDeviceCommandPublisher publisher = publisher(mqttGateway);

        publisher.publishConfiguration(DEVICE_ID, Map.of("auto_mode", true));

        ArgumentCaptor<String> payload = ArgumentCaptor.forClass(String.class);
        verify(mqttGateway).sendToMqtt(
                payload.capture(), eq("iot/garden/" + DEVICE_ID + "/config"), eq(true));
        assertTrue(new ObjectMapper().readTree(payload.getValue())
                .path("config").path("auto_mode").asBoolean());
    }

    @Test
    void doesNotRetainPumpCommands() {
        MqttGateway mqttGateway = org.mockito.Mockito.mock(MqttGateway.class);
        MqttDeviceCommandPublisher publisher = publisher(mqttGateway);

        publisher.publishPumpControl(DEVICE_ID, "OFF");

        verify(mqttGateway).sendToMqtt(
                anyString(), eq("iot/garden/" + DEVICE_ID + "/cmd"), eq(false));
    }

    private MqttDeviceCommandPublisher publisher(MqttGateway mqttGateway) {
        return new MqttDeviceCommandPublisher(
                mqttGateway,
                new ObjectMapper(),
                "tcp://localhost:1883",
                "iot/garden/{deviceId}/cmd",
                "iot/garden/{deviceId}/config");
    }
}
