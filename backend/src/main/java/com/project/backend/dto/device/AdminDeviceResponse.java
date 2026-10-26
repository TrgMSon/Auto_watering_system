package com.project.backend.dto.device;

import java.time.LocalDateTime;

import com.fasterxml.jackson.annotation.JsonProperty;

public record AdminDeviceResponse(
        String id,
        @JsonProperty("device_name") String deviceName,
        String status,
        @JsonProperty("hardware_version") String hardwareVersion,
        @JsonProperty("firmware_version") String firmwareVersion,
        @JsonProperty("last_active_at") LocalDateTime lastActiveAt) {
}
