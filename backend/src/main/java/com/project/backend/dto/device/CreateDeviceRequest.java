package com.project.backend.dto.device;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Pattern;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
public class CreateDeviceRequest {
    @NotBlank(message = "Địa chỉ MAC không được để trống")
    @Pattern(
            regexp = "(?i)(?:[0-9a-f]{2}[:-]){5}[0-9a-f]{2}",
            message = "Địa chỉ MAC phải gồm 6 cặp ký tự hex")
    private String id;

    private String deviceName;
    private String hardwareVersion;
    private String firmwareVersion;
}
