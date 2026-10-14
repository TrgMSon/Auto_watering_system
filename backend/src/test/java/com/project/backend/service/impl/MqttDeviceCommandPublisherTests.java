package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertThrows;

import org.junit.jupiter.api.Test;

import com.project.backend.exception.DeviceCommandException;

class MqttDeviceCommandPublisherTests {
    @Test
    void publishFailsWhenBrokerIsNotConfigured() {
        MqttDeviceCommandPublisher publisher =
                new MqttDeviceCommandPublisher("", "", "", "devices/{deviceId}/control");

        assertThrows(
                DeviceCommandException.class,
                () -> publisher.publish("device-1", "{\"action\":\"ON\"}"));
    }
}
