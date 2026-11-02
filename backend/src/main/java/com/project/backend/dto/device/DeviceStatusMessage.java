package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

public record DeviceStatusMessage(String status, @JsonProperty("firmware_version") String firmwareVersion) {
}
