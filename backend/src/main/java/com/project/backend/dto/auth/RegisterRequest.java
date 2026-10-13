package com.project.backend.dto.auth;

import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Size;
import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;

@Data 
@Builder 
@AllArgsConstructor 
public class RegisterRequest {
    @NotBlank (message = "Email không được để trống")
    @Email(message = "Không đúng định dạng email")
    private String email;

    @NotBlank (message="Mật khẩu không được để trống")
    @Size(min=8, message = "Mật khẩu phải có độ dài ít nhất là 8 kí tự")
    private String password;
}
