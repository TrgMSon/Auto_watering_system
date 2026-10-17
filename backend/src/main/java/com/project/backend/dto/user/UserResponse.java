package com.project.backend.dto.user;

import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
public class UserResponse {
    private int id;
    private String email;
    private String phoneNumber;
    private String role;

    // Constructor đầy đủ tham số
    public UserResponse(int id, String email, String phoneNumber, String role) {
        this.id = id;
        this.email = email;
        this.phoneNumber = phoneNumber;
        this.role = role;
    }

    // ==========================================
    // BUILDER THỦ CÔNG
    // ==========================================
    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private int id;
        private String email;
        private String phoneNumber;
        private String role;

        public Builder id(int id) {
            this.id = id;
            return this;
        }

        public Builder email(String email) {
            this.email = email;
            return this;
        }

        public Builder phoneNumber(String phoneNumber) {
            this.phoneNumber = phoneNumber;
            return this;
        }

        public Builder role(String role) {
            this.role = role;
            return this;
        }

        public UserResponse build() {
            return new UserResponse(id, email, phoneNumber, role);
        }
    }
}