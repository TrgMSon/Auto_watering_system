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
import lombok.NoArgsConstructor;

@Entity
@Table(name = "audit_log")
@Data
public class AuditLog {
    @Id
    @Column(nullable = false)
    private int id;

    private LocalDateTime timestamp;

    @ManyToOne(fetch = FetchType.LAZY)
    @JoinColumn(name = "user_id", nullable = false)
    private User user;

    @Column(name = "username")
    private String userName;

    private String action;

    @Column(name = "ip_address")
    private String ipAddress;

    private String status;

    public AuditLog() {
    }

    public AuditLog(int id, LocalDateTime timestamp, User user, String userName, String action, String ipAddress,
            String status) {
                this.id = id;
                this.timestamp = timestamp;
                this.user = user;
                this.userName = userName;
                this.action = action;
                this.ipAddress = ipAddress;
                this.status = status;
    }
}
