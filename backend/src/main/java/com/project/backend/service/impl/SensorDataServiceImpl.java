package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.List;

import org.springframework.stereotype.Service;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.dto.sensor.EnvironmentalStatsResponse;
import com.project.backend.dto.sensor.SensorLogRequest;
import com.project.backend.dto.sensor.WaterLevelStatsResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.model.Device;
import com.project.backend.model.SensorLog;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.SensorLogRepository;
import com.project.backend.service.SensorDataService;

@Service
public class SensorDataServiceImpl implements SensorDataService {
    private SensorLogRepository sensorLogRepository;
    private DeviceRepository deviceRepository;
    private ObjectMapper objectMapper;

    public SensorDataServiceImpl(SensorLogRepository sensorLogRepository, DeviceRepository deviceRepository,
            ObjectMapper objectMapper) {
        this.sensorLogRepository = sensorLogRepository;
        this.deviceRepository = deviceRepository;
        this.objectMapper = objectMapper;
    }

    @Override
    public void recordSensorLog(String topicName, String payload) {
        try {
            String deviceId = deviceIdFromTopic(topicName);
            if (deviceId.isBlank())
                return;

            SensorLogRequest request = objectMapper.readValue(payload, SensorLogRequest.class);
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
        } catch (Exception e) {
            e.printStackTrace();
        }
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
        return deviceId.isBlank() || deviceId.contains("/") ? null : deviceId;
    }
}
