package com.project.backend.dto.device;

import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data 
@Builder 
@NoArgsConstructor 
public class DeviceRequest {
    private String hardwareVersion;
    private String firmwareVersion;
    private String status;
    
    public DeviceRequest(String hardwareVersion, String firmwareVersion, String status) {
        this.hardwareVersion = hardwareVersion;
        this.firmwareVersion = firmwareVersion;
        this.status = status;
    }
}
