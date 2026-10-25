package com.project.backend.dto.sensor;

import java.time.LocalDateTime;

import lombok.AllArgsConstructor;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;

@Data
@Builder
@NoArgsConstructor
@AllArgsConstructor
public class WaterLevelStatsResponse {
    private LocalDateTime timeLabel;
    private Float avgWaterLevel;
}