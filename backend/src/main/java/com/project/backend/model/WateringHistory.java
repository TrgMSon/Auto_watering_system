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
    private LocalDateTime startTime;

    @Column(name = "end_time")
    private LocalDateTime endTime;

    private String status;

    public WateringHistory() {
    }

    public WateringHistory(int id, Device device, User user, LocalDateTime startTime, LocalDateTime endTime, String status) {
        this.id = id;
        this.device = device;
        this.user = user;
        this.startTime = startTime;
        this.endTime = endTime;
        this.status = status;
    }

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private int id;
        private Device device;
        private User user;
        private LocalDateTime startTime;
        private LocalDateTime endTime;
        private String status;

        public Builder id(int id) {
            this.id = id;
            return this;
        }

        public Builder device(Device device) {
            this.device = device;
            return this;
        }

        public Builder user(User user) {
            this.user = user;
            return this;
        }

        public Builder startTime(LocalDateTime startTime) {
            this.startTime = startTime;
            return this;
        }

        public Builder endTime(LocalDateTime endTime) {
            this.endTime = endTime;
            return this;
        }

        public Builder status(String status) {
            this.status = status;
            return this;
        }

        public WateringHistory build() {
            return new WateringHistory(id, device, user, startTime, endTime, status);
        }
    }
}
