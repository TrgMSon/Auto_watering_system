package com.project.backend.model;

import java.time.LocalDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import lombok.Data;
import lombok.NoArgsConstructor;

@Entity
@Data
@NoArgsConstructor
public class Device {
    @Id
    @Column(nullable = false)
    private String id;

    @Column(name = "device_name")
    private String deviceName;

    @Column(name = "hardware_version")
    private String hardwareVersion;

    @Column(name = "firmware_version")
    private String firmwareVersion;

    private String status;

    @Column(name = "last_active_at")
    private LocalDateTime lastActiveAt;

    @Column(name = "created_at")
    private LocalDateTime createdAt;

    @Column(name = "updated_at")
    private LocalDateTime updatedAt;

    @Column(name = "ble_pass_key")
    private String blePassKey;

    public Device(String id, String deviceName, String hardwareVesrsion, String firmwareVersion, String status,
            LocalDateTime lastActiveAt, LocalDateTime createdAt, LocalDateTime updatedAt) {
                this.id = id;
                this.deviceName = deviceName;
                this.hardwareVersion = hardwareVesrsion;
                this.firmwareVersion = firmwareVersion;
                this.status = status;
                this.lastActiveAt = lastActiveAt;
                this.createdAt = createdAt;
                this.updatedAt = updatedAt;
    }

}
