package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

public record CommandAcknowledgmentResponse(
        @JsonProperty("command_id") String commandId,
        @JsonProperty("device_id") String deviceId,
        String status,
        String message,
        String timestamp) {
}
