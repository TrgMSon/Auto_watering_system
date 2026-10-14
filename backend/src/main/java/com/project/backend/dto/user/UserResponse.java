package com.project.backend.dto.user;

import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
@Builder
public class UserResponse {
    private int id;
    private String email;
    private String phoneNumber;
    private String role;

    public UserResponse(int id, String email, String phoneNumber, String role) {
        this.id = id;
        this.email = email;
        this.phoneNumber = phoneNumber;
        this.role = role;
    }
}
