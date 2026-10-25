package com.project.backend.dto.sensor;

import java.time.LocalDateTime;

import com.fasterxml.jackson.annotation.JsonProperty;

public record SensorReadingResponse(
        int id,
        @JsonProperty("device_id") String deviceId,
        @JsonProperty("soil_moisture") Float soilMoisture,
        Float temperature,
        Float humidity,
        @JsonProperty("water_level") Float waterLevel,
        @JsonProperty("recorded_at") LocalDateTime recordedAt) {
}
