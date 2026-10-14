package com.project.backend.model;

import java.time.LocalDateTime;

import com.project.backend.model.enums.AccountStatus;
import com.project.backend.model.enums.Role;
import com.project.backend.model.enums.RoleConverter;

import jakarta.persistence.Column;
import jakarta.persistence.Convert;
import jakarta.persistence.Entity;
import jakarta.persistence.EnumType;
import jakarta.persistence.Enumerated;
import jakarta.persistence.Id;
import lombok.Builder;
import lombok.Data;

@Entity
@Data
@Builder 
public class User {
    @Id
    @Column(nullable = false)
    private int id;

    private String email;

    @Column(name = "password_hash")
    private String passwordHash;

    @Column(name = "phone_number")
    private String phoneNumber;

    @Convert(converter = RoleConverter.class)
    private Role role;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private AccountStatus status;

    @Column(name = "created_at")
    private LocalDateTime createdAt;

    public User() {
    }

    public User(int id, String email, String passwordHash, String phoneNumber, Role role, AccountStatus status, LocalDateTime createdAt) {
        this.id = id;
        this.email = email;
        this.passwordHash = passwordHash;
        this.phoneNumber = phoneNumber;
        this.role = role;
        this.status = status;
        this.createdAt = createdAt;
    }
}
