package com.project.backend.model;

import java.time.LocalTime;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.FetchType;
import jakarta.persistence.Id;
import jakarta.persistence.JoinColumn;
import jakarta.persistence.ManyToOne;
import jakarta.persistence.Table;
import lombok.Data;

@Entity
@Table(name = "watering_history")
@Data
public class WateringHistory {
    @Id
    @Column(nullable = false)
    private int id;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "device_id")
    private Device device;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "user_id")
    private User user;

    @Column(name = "start_time")
    private LocalTime startTime;

    @Column(name = "end_time")
    private LocalTime endTime;

    private String status;

    public WateringHistory() {
    }

    public WateringHistory(int id, Device device, User user, LocalTime startTime, LocalTime endTime, String status) {
        this.id = id;
        this.device = device;
        this.user = user;
        this.startTime = startTime;
        this.endTime = endTime;
        this.status = status;
    }
}
