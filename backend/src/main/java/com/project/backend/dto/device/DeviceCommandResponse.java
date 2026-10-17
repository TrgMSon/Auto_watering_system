package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

public record DeviceCommandResponse(
        @JsonProperty("device_id") String deviceId,
        String action,
        String status,
        @JsonProperty("command_id") String commandId) {
}
