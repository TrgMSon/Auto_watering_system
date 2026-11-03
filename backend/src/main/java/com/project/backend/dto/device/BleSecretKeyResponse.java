package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

public record BleSecretKeyResponse(@JsonProperty("ble_secret_key") String bleSecretKey) {
}
