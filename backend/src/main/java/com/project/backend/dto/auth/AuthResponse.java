package com.project.backend.dto.auth;

import com.project.backend.dto.user.UserResponse;

import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
// trả về khi  login thành công
public class AuthResponse {
    private String accessToken;
    private String refreshToken;
    private String role;
    private UserResponse userResponse;

    public AuthResponse(String accessToken, String refreshToken, String role, UserResponse userResponse) {
        this.accessToken = accessToken;
        this.refreshToken = refreshToken;
        this.role = role;
        this.userResponse = userResponse;
    }
}
