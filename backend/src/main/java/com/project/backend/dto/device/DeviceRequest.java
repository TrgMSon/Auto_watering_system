package com.project.backend.dto.device;

import lombok.Data;
import lombok.NoArgsConstructor;

@Data 
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

    public static Builder builder() {
        return new Builder();
    }

    public static class Builder {
        private String hardwareVersion;
        private String firmwareVersion;
        private String status;

        public Builder hardwareVersion(String hardwareVersion) {
            this.hardwareVersion = hardwareVersion;
            return this;
        }

        public Builder firmwareVersion(String firmwareVersion) {
            this.firmwareVersion = firmwareVersion;
            return this;
        }

        public Builder status(String status) {
            this.status = status;
            return this;
        }

        public DeviceRequest build() {
            return new DeviceRequest(hardwareVersion, firmwareVersion, status);
        }
    }
}