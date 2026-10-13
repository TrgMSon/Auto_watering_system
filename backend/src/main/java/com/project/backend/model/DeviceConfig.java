package com.project.backend.model;

import java.time.LocalDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.OneToOne;
import jakarta.persistence.Table;
import lombok.AllArgsConstructor;
import lombok.Data;

@Entity
@Table(name = "device_config")
@Data
@AllArgsConstructor
public class DeviceConfig {
    @Id
    @OneToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id", nullable = false)
    private Device device;

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
}
