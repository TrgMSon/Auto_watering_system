package com.project.backend.dto.device;

import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@NoArgsConstructor
public class RegisterDeviceRequest {
    private int userId;

    public RegisterDeviceRequest(int userId) {
        this.userId = userId;
    }
}
