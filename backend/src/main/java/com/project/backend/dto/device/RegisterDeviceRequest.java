package com.project.backend.dto.device;

import lombok.Data;

@Data 
public class RegisterDeviceRequest {
    private int userId;

    public RegisterDeviceRequest(int userId) {
        this.userId = userId;
    }
}
