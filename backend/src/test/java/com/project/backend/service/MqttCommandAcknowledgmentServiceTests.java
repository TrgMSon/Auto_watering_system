package com.project.backend.service;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.junit.jupiter.api.Test;

class MqttCommandAcknowledgmentServiceTests {
    private final MqttCommandAcknowledgmentService service =
            new MqttCommandAcknowledgmentService(new ObjectMapper());

    @Test
    void correlatesDeviceAcknowledgmentByDeviceAndCommandId() {
        service.register("device-1", "CMD-123");

        service.handleMessage(
                "iot/garden/device-1/ack",
                """
                {
                  "command_id": "CMD-123",
                  "device_id": "device-1",
                  "status": "SUCCESS",
                  "message": "Pump started"
                }
                """);

        var result = service.find("device-1", "CMD-123");
        assertTrue(result.isPresent());
        assertEquals("SUCCEEDED", result.get().status());
        assertEquals("Pump started", result.get().message());
    }

    @Test
    void ignoresAcknowledgmentFromDifferentDevice() {
        service.register("device-1", "CMD-123");

        service.handleMessage(
                "iot/garden/device-2/ack",
                """
                {
                  "command_id": "CMD-123",
                  "device_id": "device-2",
                  "status": "SUCCESS"
                }
                """);

        assertEquals("PENDING", service.find("device-1", "CMD-123").orElseThrow().status());
    }

    @Test
    void recordsExecutionFailure() {
        service.register("device-1", "CMD-123");

        service.handleMessage(
                "iot/garden/device-1/ack",
                """
                {
                  "command_id": "CMD-123",
                  "device_id": "device-1",
                  "status": "FAILED",
                  "message": "Water level too low"
                }
                """);

        var result = service.find("device-1", "CMD-123").orElseThrow();
        assertEquals("FAILED", result.status());
        assertEquals("Water level too low", result.message());
    }
}
