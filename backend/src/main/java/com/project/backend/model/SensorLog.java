package com.project.backend.model;

import java.time.LocalDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.Table;
import lombok.Data;

@Entity
@Table(name = "sensor_log")
@Data
public class SensorLog {
    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    @Column(nullable = false)
    private int id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id")
    private Device device;

    @Column(name = "soil_moisture")
    private Float soilMoisture;

    private Float temperature;
    private Float humidity;

    @Column(name = "water_level")
    private Float waterLevel;

    @Column(name = "recorded_at")
    private LocalDateTime recordedAt;

    public SensorLog() {
    }

    public SensorLog(int id, Device device, Float soilMoisture, Float temperature, Float humidity, Float waterLevel, LocalDateTime recordedAt) {
        this.id = id;
        this.device = device;
        this.soilMoisture = soilMoisture;
        this.temperature = temperature;
        this.humidity = humidity;
        this.waterLevel = waterLevel;
        this.recordedAt = recordedAt;
    }

}
