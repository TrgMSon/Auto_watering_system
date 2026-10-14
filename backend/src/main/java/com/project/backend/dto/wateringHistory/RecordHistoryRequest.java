package com.project.backend.dto.wateringHistory;

import java.time.LocalDateTime;

import lombok.Data;

@Data 
public class RecordHistoryRequest {
    private String deviceId;
    private int userId;
    private String status;
    private LocalDateTime startTime;
    private LocalDateTime endTime;

    public RecordHistoryRequest(String deviceId, int userId, String status, LocalDateTime starTime, LocalDateTime endTime) {
        this.deviceId = deviceId;
        this.userId = userId;
        this.status = status;
        this.startTime = starTime;
        this.endTime = endTime;
    }
}
