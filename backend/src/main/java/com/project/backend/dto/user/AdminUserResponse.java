package com.project.backend.dto.user;

import java.time.LocalDateTime;

import com.fasterxml.jackson.annotation.JsonProperty;

public record AdminUserResponse(
        int id,
        String email,
        @JsonProperty("phone_number") String phoneNumber,
        String role,
        String status,
        @JsonProperty("created_at") LocalDateTime createdAt) {
}
