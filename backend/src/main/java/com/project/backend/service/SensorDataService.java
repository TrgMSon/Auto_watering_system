package com.project.backend.service;

import java.util.List;

import com.project.backend.dto.sensor.EnvironmentalStatsResponse;
import com.project.backend.dto.sensor.WaterLevelStatsResponse;

public interface SensorDataService {
    public void recordSensorLog(String topicName, String payload);
    public List<WaterLevelStatsResponse> getWaterLevelStats(String deviceId, String range);
    public List<EnvironmentalStatsResponse> getEnvironmentalStats(String deviceId, String range);
}
