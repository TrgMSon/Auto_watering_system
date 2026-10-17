package com.project.backend.service;

import java.time.LocalDateTime;

public interface WateringHistoryService {
    public void recordStartWatering(String deviceId, LocalDateTime ackTime);
    public void recordEndWatering(String deviceId, LocalDateTime ackTime);
}
