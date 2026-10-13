package com.project.backend.dto.auth;

import lombok.Data;

@Data
public class LogoutRequest {
    private String refreshToken;
}
