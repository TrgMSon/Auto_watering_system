package com.project.backend.controller;

import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.dto.sensor.EnvironmentalStatsResponse;
import com.project.backend.dto.sensor.WaterLevelStatsResponse;
import com.project.backend.service.SensorDataService;

import java.util.List;

import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;

@RestController
@RequestMapping("/api/statistics")
public class StatisticController {
    private final SensorDataService sensorDataService;

    public StatisticController(SensorDataService sensorDataService) {
        this.sensorDataService = sensorDataService;
    }

    @GetMapping("/environmental")
    @PreAuthorize("hasRole('USER')")
    public ResponseEntity<List<EnvironmentalStatsResponse>> getEnvironmentalStats(
            @RequestParam("deviceId") String deviceId,
            @RequestParam(name = "range", defaultValue = "24h") String range,
            Authentication authentication) {

        List<EnvironmentalStatsResponse> stats = sensorDataService.getEnvironmentalStats(
                deviceId, (Integer) authentication.getDetails(), range);
        return ResponseEntity.ok(stats);
    }

    @GetMapping("/water-level")
    @PreAuthorize("hasRole('USER')")
    public ResponseEntity<List<WaterLevelStatsResponse>> getWaterLevelStats(
            @RequestParam("deviceId") String deviceId,
            @RequestParam(name = "range", defaultValue = "24h") String range,
            Authentication authentication) {

        List<WaterLevelStatsResponse> stats = sensorDataService.getWaterLevelStats(
                deviceId, (Integer) authentication.getDetails(), range);
        return ResponseEntity.ok(stats);
    }

}
