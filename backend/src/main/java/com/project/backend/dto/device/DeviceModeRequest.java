package com.project.backend.dto.device;

import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class DeviceModeRequest {
    @NotBlank
    private String mode;
}
