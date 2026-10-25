package com.project.backend.controller;

import java.time.LocalDateTime;
import java.util.List;

import org.springframework.format.annotation.DateTimeFormat;
import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.device.DeviceSummaryResponse;
import com.project.backend.dto.sensor.SensorReadingResponse;
import com.project.backend.service.DeviceService;

@RestController
@RequestMapping("/api/devices")
public class MobileDeviceController {
    private final DeviceService deviceService;

    public MobileDeviceController(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @GetMapping
    @PreAuthorize("hasAnyRole('USER', 'OWNER')")
    public ResponseEntity<ApiResponse<List<DeviceSummaryResponse>>> getDevices(
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                deviceService.getUserDevices(userId(authentication))));
    }

    @GetMapping("/{id}/telemetry")
    @PreAuthorize("hasAnyRole('USER', 'OWNER')")
    public ResponseEntity<ApiResponse<List<SensorReadingResponse>>> getSensorReadings(
            @PathVariable String id,
            @RequestParam @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) LocalDateTime from,
            @RequestParam @DateTimeFormat(iso = DateTimeFormat.ISO.DATE_TIME) LocalDateTime to,
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                deviceService.getSensorReadings(id, userId(authentication), from, to)));
    }

    @GetMapping("/{id}/telemetry/latest")
    @PreAuthorize("hasAnyRole('USER', 'OWNER')")
    public ResponseEntity<ApiResponse<SensorReadingResponse>> getLatestSensorReading(
            @PathVariable String id, Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                deviceService.getLatestSensorReading(id, userId(authentication))));
    }

    private int userId(Authentication authentication) {
        return (Integer) authentication.getDetails();
    }
}
