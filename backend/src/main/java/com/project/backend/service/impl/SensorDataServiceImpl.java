package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.dto.sensor.EnvironmentalStatsResponse;
import com.project.backend.dto.sensor.SensorLogRequest;
import com.project.backend.dto.sensor.WaterLevelStatsResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.model.Device;
import com.project.backend.model.SensorLog;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.SensorLogRepository;
import com.project.backend.service.DeviceLifecycleService;
import com.project.backend.service.SensorDataService;

@Service
public class SensorDataServiceImpl implements SensorDataService {
    private static final Logger logger = LoggerFactory.getLogger(SensorDataServiceImpl.class);

    private final SensorLogRepository sensorLogRepository;
    private final DeviceRepository deviceRepository;
    private final ObjectMapper objectMapper;
    private final DeviceLifecycleService deviceLifecycleService;

    public SensorDataServiceImpl(SensorLogRepository sensorLogRepository, DeviceRepository deviceRepository,
            ObjectMapper objectMapper, DeviceLifecycleService deviceLifecycleService) {
        this.sensorLogRepository = sensorLogRepository;
        this.deviceRepository = deviceRepository;
        this.objectMapper = objectMapper;
        this.deviceLifecycleService = deviceLifecycleService;
    }

    @Override
    @Transactional
    public void recordSensorLog(String topicName, String payload) {
        String deviceId = deviceIdFromTopic(topicName);
        if (deviceId == null) {
            logger.warn("Ignoring malformed telemetry topic: {}", topicName);
            return;
        }

        SensorLogRequest request;
        try {
            request = objectMapper.readValue(payload, SensorLogRequest.class);
        } catch (JsonProcessingException exception) {
            logger.warn("Ignoring malformed telemetry payload for {}", deviceId, exception);
            return;
        }

        if (!deviceLifecycleService.recordTelemetry(deviceId)) {
            return;
        }

        Device device = deviceRepository.findById(deviceId)
                .orElseThrow(() -> new ConflictException("Thiết bị không tồn tại"));
        SensorLog sensorLog = new SensorLog();
        sensorLog.setDevice(device);
        sensorLog.setSoilMoisture(request.getSoilMoisture());
        sensorLog.setTemperature(request.getTemperature());
        sensorLog.setHumidity(request.getHumidity());
        sensorLog.setWaterLevel(request.getWaterLevel());
        sensorLog.setRecordedAt(request.getRecordedAt() != null ? request.getRecordedAt() : LocalDateTime.now());
        sensorLogRepository.save(sensorLog);
    }

    @Override
    public List<EnvironmentalStatsResponse> getEnvironmentalStats(String deviceId, String range) {
        // 1. Tính toán thời gian bắt đầu dựa vào range
        LocalDateTime startTime = calculateStartTime(range);

        // 2. Gọi repository với cả 2 tham số
        List<Object[]> results = sensorLogRepository.getHourlyEnvironmentalStats(deviceId, startTime);

        List<EnvironmentalStatsResponse> responseList = new ArrayList<>();
        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

        for (Object[] row : results) {
            String timeStr = (String) row[0];
            LocalDateTime timeLabel = (timeStr != null) ? LocalDateTime.parse(timeStr, formatter) : null;

            Float avgSoil = row[1] != null ? ((Number) row[1]).floatValue() : null;
            Float avgTemp = row[2] != null ? ((Number) row[2]).floatValue() : null;
            Float avgHumid = row[3] != null ? ((Number) row[3]).floatValue() : null;

            responseList.add(new EnvironmentalStatsResponse(timeLabel, avgSoil, avgTemp, avgHumid));
        }

        return responseList;
    }

    @Override
    public List<WaterLevelStatsResponse> getWaterLevelStats(String deviceId, String range) {
        // 1. Tính toán thời gian bắt đầu từ range (24h, 7d, 30d)
        LocalDateTime startTime = calculateStartTime(range);

        // 2. Truy vấn dữ liệu từ database
        List<Object[]> results = sensorLogRepository.getHourlyWaterLevelStats(deviceId, startTime);

        List<WaterLevelStatsResponse> responseList = new ArrayList<>();
        DateTimeFormatter formatter = DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

        // 3. Ánh xạ thủ công từ Object[] sang DTO
        for (Object[] row : results) {
            String timeStr = (String) row[0];
            LocalDateTime timeLabel = (timeStr != null) ? LocalDateTime.parse(timeStr, formatter) : null;

            Float avgWaterLevel = row[1] != null ? ((Number) row[1]).floatValue() : null;

            responseList.add(new WaterLevelStatsResponse(timeLabel, avgWaterLevel));
        }

        return responseList;
    }

    // Hàm phụ trợ tính mốc thời gian
    private LocalDateTime calculateStartTime(String range) {
        LocalDateTime now = LocalDateTime.now();
        return switch (range.toLowerCase()) {
            case "7d" -> now.minusDays(7);
            case "30d" -> now.minusDays(30);
            default -> now.minusHours(24); // Mặc định 24h
        };
    }

    private String deviceIdFromTopic(String topic) {
        if (topic == null || !topic.startsWith("iot/garden/") || !topic.endsWith("/telemetry")) {
            return null;
        }
        String deviceId = topic.substring("iot/garden/".length(), topic.length() - "/telemetry".length());
        return deviceId.isBlank() || deviceId.contains("/")
                ? null
                : deviceId.trim().replace('-', ':').toUpperCase(java.util.Locale.ROOT);
    }
}
