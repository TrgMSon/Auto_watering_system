package com.project.backend.model;

import java.time.LocalDateTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.Table;
import lombok.Data;

@Entity
@Table(name = "user_device")
@Data
public class UserDevice {
    @Id
    @Column(nullable = false)
    private int id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "user_id", nullable = false)
    private User user;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id", nullable = false)
    private Device device;

    private String permission;

    @Column(name = "created_at")
    private LocalDateTime createdAt;

    public UserDevice() {
    }

    public UserDevice(int id, User user, Device device, String permission, LocalDateTime createdAt) {
        this.id = id;
        this.user = user;
        this.device = device;
        this.permission = permission;
        this.createdAt = createdAt;
    }
}
