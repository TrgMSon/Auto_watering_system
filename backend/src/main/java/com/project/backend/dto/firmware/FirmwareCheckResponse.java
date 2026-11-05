package com.project.backend.dto.firmware;

import com.fasterxml.jackson.annotation.JsonProperty;

public record FirmwareCheckResponse(
        @JsonProperty("current_version") String currentVersion,
        @JsonProperty("latest_version") String latestVersion,
        @JsonProperty("update_available") boolean updateAvailable,
        @JsonProperty("sha256") String sha256,
        @JsonProperty("size_bytes") Long sizeBytes,
        @JsonProperty("download_url") String downloadUrl) {
}
