package com.project.backend.dto.device;

import lombok.Data;

@Data 
public class PumpControlRequest {
    private String state; // "ON" hoặc "OFF"
    private int durationSeconds;

    public PumpControlRequest(String state, int durationSeconds) {
        this.state = state;
        this.durationSeconds = durationSeconds;
    }
}
