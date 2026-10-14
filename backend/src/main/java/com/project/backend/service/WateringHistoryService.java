package com.project.backend.service;

import com.project.backend.dto.wateringHistory.RecordHistoryRequest;

public interface WateringHistoryService {
    public void recordWateringHistory(RecordHistoryRequest request);
}
