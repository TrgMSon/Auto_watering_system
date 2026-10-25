package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

public record DeviceSummaryResponse(
        String id,
        @JsonProperty("device_name") String deviceName,
        String status,
        String permission) {
}
