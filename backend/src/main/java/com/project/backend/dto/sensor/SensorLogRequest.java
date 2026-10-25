package com.project.backend.dto.sensor;

import java.time.LocalDateTime;

import lombok.Data;

@Data 
public class SensorLogRequest {
    private String deviceId;
    private Float soilMoisture;
    private Float temperature;
    private Float humidity;
    private Float waterLevel;
    private LocalDateTime recordedAt;

    public SensorLogRequest() {
    }

    public SensorLogRequest(String deviceId, Float soilMoisture, Float temperature, Float humidity, Float waterLevel, LocalDateTime recordedAt) {
        this.deviceId = deviceId;
        this.soilMoisture = soilMoisture;
        this.temperature = temperature;
        this.humidity = humidity;
        this.waterLevel = waterLevel;
        this.recordedAt = recordedAt;
    }
}
