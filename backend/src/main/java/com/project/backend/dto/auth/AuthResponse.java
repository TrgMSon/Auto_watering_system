package com.project.backend.dto.auth;

import com.project.backend.dto.user.UserResponse;

import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
// trả về khi login thành công
public class AuthResponse {
    private String accessToken;
    private String refreshToken;
    private String role;
    private UserResponse userResponse;

    // Constructor đầy đủ tham số
    public AuthResponse(String accessToken, String refreshToken, String role, UserResponse userResponse) {
        this.accessToken = accessToken;
        this.refreshToken = refreshToken;
        this.role = role;
        this.userResponse = userResponse;
    }

    // ==========================================
    // BUILDER THỦ CÔNG
    // ==========================================
    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private String accessToken;
        private String refreshToken;
        private String role;
        private UserResponse userResponse;

        public Builder accessToken(String accessToken) {
            this.accessToken = accessToken;
            return this;
        }

        public Builder refreshToken(String refreshToken) {
            this.refreshToken = refreshToken;
            return this;
        }

        public Builder role(String role) {
            this.role = role;
            return this;
        }

        public Builder userResponse(UserResponse userResponse) {
            this.userResponse = userResponse;
            return this;
        }

        public AuthResponse build() {
            return new AuthResponse(accessToken, refreshToken, role, userResponse);
        }
    }
}