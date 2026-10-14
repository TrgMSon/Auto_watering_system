package com.project.backend.model;

import java.time.LocalDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Table;
import lombok.Data;
import lombok.NoArgsConstructor;

@Entity
@Table(name = "device_config")
@Data
@NoArgsConstructor
public class DeviceConfig {
    @Id
    @Column(name = "device_id", nullable = false)
    private String deviceId;

    @Column(name = "auto_mode")
    private int autoMode;

    @Column(name = "soil_moisture_min")
    private float soilMoistureMin;

    @Column(name = "soil_moisture_max")
    private float soilMoistureMax;

    @Column(name = "max_watering_duration")
    private int maxWateringDuration;

    @Column(name = "updated_at")
    private LocalDateTime updatedAt;

    @Column(name = "air_humidity_min")
    private float airHumidityMin;

    @Column(name = "air_humidity_max")
    private float airHumidityMax;

    @Column(name = "air_temperature_min")
    private float airTemperatureMin;

    @Column(name = "air_temperature_max")
    private float airTemperatureMax;

    public DeviceConfig(String deviceId, int autoMode, float soilMoistureMin, float soiMoistureMax,
            int maxWateringDuration, LocalDateTime updatedAt, float airHumidityMin, float airHumidityMax,
            float airTemperatureMin, float airTemperatureMax) {
                this.deviceId = deviceId;
                this.autoMode = autoMode;
                this.soilMoistureMin = soilMoistureMin;
                this.soilMoistureMax = soiMoistureMax;
                this.maxWateringDuration = maxWateringDuration;
                this.updatedAt = updatedAt;
                this.airHumidityMax = airHumidityMax;
                this.airHumidityMin = airHumidityMin;
                this.airTemperatureMax = airTemperatureMax;
                this.airTemperatureMin = airTemperatureMin;
    }
}
