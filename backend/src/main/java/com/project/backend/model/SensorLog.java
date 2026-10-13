package com.project.backend.model;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.Table;
import lombok.AllArgsConstructor;
import lombok.Data;

@Entity
@Table(name = "sensor_log")
@Data
@AllArgsConstructor
public class SensorLog {
    @Id
    @Column(nullable = false)
    private int id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id")
    private Device device;

    @Column(name = "soil_moisture")
    private float soilMoisture;

    private float temperature;
    private float humidity;

    @Column(name = "water_level")
    private float waterLevel;

    @Column(name = "recorded_at")
    private float recordedAt;

}
