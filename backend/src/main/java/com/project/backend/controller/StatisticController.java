package com.project.backend.controller;

import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.dto.sensor.EnvironmentalStatsResponse;
import com.project.backend.dto.sensor.WaterLevelStatsResponse;
import com.project.backend.service.impl.SensorDataServiceImpl;

import java.util.List;

import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestParam;


@RestController 
@RequestMapping("/api/admin/statistics")
public class StatisticController {
    private SensorDataServiceImpl sensorDataService;

    public StatisticController(SensorDataServiceImpl sensorDataService) {
        this.sensorDataService = sensorDataService;
    }

    @GetMapping("/environmental")
    public ResponseEntity<List<EnvironmentalStatsResponse>> getEnvironmentalStats(
            @RequestParam("deviceId") String deviceId,
            @RequestParam(name = "range", defaultValue = "24h") String range) {
        
        List<EnvironmentalStatsResponse> stats = sensorDataService.getEnvironmentalStats(deviceId, range);
        return ResponseEntity.ok(stats);
    }

    @GetMapping("/water-level")
    public ResponseEntity<List<WaterLevelStatsResponse>> getWaterLevelStats(
            @RequestParam("deviceId") String deviceId,
            @RequestParam(name = "range", defaultValue = "24h") String range) {
        
        List<WaterLevelStatsResponse> stats = sensorDataService.getWaterLevelStats(deviceId, range);
        return ResponseEntity.ok(stats);
    }
    
}
