package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertThrows;

import java.time.Instant;
import java.util.Map;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.Test;
import org.springframework.messaging.MessagingException;

import com.project.backend.config.MqttGateway;
import com.project.backend.exception.DeviceCommandException;
import com.project.backend.service.MqttCommandAcknowledgmentService;

import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

class MqttDeviceCommandPublisherTests {
    private final ObjectMapper objectMapper = new ObjectMapper();

    @Test
    void publishFailsWhenBrokerIsNotConfigured() {
        MqttDeviceCommandPublisher publisher =
                new MqttDeviceCommandPublisher("", "iot/garden/{deviceId}/cmd");

        assertThrows(
                DeviceCommandException.class,
                () -> publisher.publishPumpControl("device-1", "ON", 300));
    }

    @Test
    void publishesUsingManagedMqttGateway() throws Exception {
        MqttGateway gateway = mock(MqttGateway.class);
        MqttCommandAcknowledgmentService acknowledgmentService =
                new MqttCommandAcknowledgmentService(objectMapper);
        MqttDeviceCommandPublisher publisher = new MqttDeviceCommandPublisher(
                gateway, objectMapper, acknowledgmentService, "ssl://broker",
                "iot/garden/{deviceId}/cmd",
                "iot/garden/{deviceId}/config");

        String commandId = publisher.publishPumpControl("device-1", "ON", 300);

        org.mockito.ArgumentCaptor<String> payloadCaptor =
                org.mockito.ArgumentCaptor.forClass(String.class);
        verify(gateway).sendToMqtt(payloadCaptor.capture(), eq("iot/garden/device-1/cmd"));
        assertEquals(commandId, objectMapper.readTree(payloadCaptor.getValue())
                .get("command_id").asText());
        acknowledgmentService.handleMessage(
                "iot/garden/device-1/ack",
                """
                {"command_id":"%s","device_id":"device-1","status":"SUCCESS"}
                """.formatted(commandId));
        assertEquals("SUCCEEDED",
                acknowledgmentService.find("device-1", commandId).orElseThrow().status());
    }

    @Test
    void retriesPublishUpToThreeTimesAfterGatewayFailure() {
        MqttGateway gateway = mock(MqttGateway.class);
        doThrow(new MessagingException("publish failed"))
                .doThrow(new MessagingException("publish failed"))
                .doNothing()
                .when(gateway).sendToMqtt(anyString(), anyString());
        MqttDeviceCommandPublisher publisher = new MqttDeviceCommandPublisher(
                gateway, objectMapper, "ssl://broker", "iot/garden/{deviceId}/cmd",
                "iot/garden/{deviceId}/config");

        publisher.publishPumpControl("device-1", "ON", 300);

        verify(gateway, times(3)).sendToMqtt(anyString(), eq("iot/garden/device-1/cmd"));
    }

    @Test
    void buildsPumpCommandUsingTheProtocolEnvelope() {
        MqttDeviceCommandPublisher publisher =
                new MqttDeviceCommandPublisher("", "iot/garden/{deviceId}/cmd");

        var payload = publisher.buildPumpControlPayload("A4CF128FC130", "ON", 300);

        assertEquals("A4CF128FC130", payload.get("device_id"));
        assertEquals("PUMP_CONTROL", payload.get("action"));
        assertNotNull(payload.get("command_id"));
        assertNotNull(Instant.parse((String) payload.get("timestamp")));
        assertEquals(
                Map.of("state", "ON", "duration_seconds", 300),
                objectMapper.convertValue(payload.get("params"), Map.class));
    }

    @Test
    void buildsConfigurationWithProtocolFieldNames() {
        MqttDeviceCommandPublisher publisher =
                new MqttDeviceCommandPublisher("", "iot/garden/{deviceId}/cmd");
        Map<String, Object> config = Map.of(
                "auto_mode", true,
                "soil_moisture_min", 30.0,
                "soil_moisture_max", 70.0,
                "max_watering_duration_minutes", 10,
                "min_water_level_alert", 5);

        var payload = publisher.buildConfigurationPayload("A4CF128FC130", config);
        Map<?, ?> nestedConfig =
                objectMapper.convertValue(payload.get("config"), Map.class);

        assertEquals("A4CF128FC130", payload.get("device_id"));
        assertNotNull(payload.get("command_id"));
        assertNotNull(Instant.parse((String) payload.get("timestamp")));
        assertEquals(5, nestedConfig.get("min_water_level_alert"));
        assertEquals(10, nestedConfig.get("max_watering_duration_minutes"));
    }
}
