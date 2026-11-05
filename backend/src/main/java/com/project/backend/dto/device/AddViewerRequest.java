package com.project.backend.dto.device;

import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;

public record AddViewerRequest(
        @NotBlank(message = "Email không được để trống")
        @Email(message = "Không đúng định dạng email")
        String email) {
}
