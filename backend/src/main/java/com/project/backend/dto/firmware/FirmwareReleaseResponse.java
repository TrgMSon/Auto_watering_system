package com.project.backend.dto.firmware;

import java.time.LocalDateTime;

import com.fasterxml.jackson.annotation.JsonProperty;

public record FirmwareReleaseResponse(
        String version,
        String notes,
        @JsonProperty("sha256") String sha256,
        @JsonProperty("size_bytes") long sizeBytes,
        @JsonProperty("created_at") LocalDateTime createdAt) {
}
