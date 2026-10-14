package com.project.backend.dto.auth;

import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import lombok.Builder;
import lombok.Data;

@Data
@Builder
public class LoginRequest {
    @NotBlank (message = "Email không được để trống")
    @Email(message = "Không đúng định dạng email")
    private String email;

    @NotBlank (message="Mật khẩu không được để trống")
    private String password;

    public LoginRequest(String email, String password) {
        this.email = email;
        this.password = password;
    }
}
