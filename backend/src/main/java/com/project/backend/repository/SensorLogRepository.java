package com.project.backend.repository;

import java.time.LocalDateTime;
import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.SensorLog;

public interface SensorLogRepository extends JpaRepository<SensorLog, Integer> {
    @Query(value = """
            SELECT
                DATE_FORMAT(s.recorded_at, '%Y-%m-%d %H:00:00') AS time_label,
                AVG(s.soil_moisture) AS avg_soil,
                AVG(s.temperature) AS avg_temp,
                AVG(s.humidity) AS avg_humid
            FROM sensor_log s
            WHERE s.device_id = ?1
              AND s.recorded_at >= ?2
              AND (s.soil_moisture IS NOT NULL OR s.temperature IS NOT NULL OR s.humidity IS NOT NULL)
            GROUP BY DATE_FORMAT(s.recorded_at, '%Y-%m-%d %H:00:00')
            ORDER BY time_label ASC
            """, nativeQuery = true)
    List<Object[]> getHourlyEnvironmentalStats(String deviceId, LocalDateTime startTime);

    @Query(value = """
            SELECT
                DATE_FORMAT(s.recorded_at, '%Y-%m-%d %H:00:00') AS time_label,
                AVG(s.water_level) AS avg_water_level
            FROM sensor_log s
            WHERE s.device_id = ?1
              AND s.recorded_at >= ?2
              AND s.water_level IS NOT NULL
            GROUP BY DATE_FORMAT(s.recorded_at, '%Y-%m-%d %H:00:00')
            ORDER BY time_label ASC
            """, nativeQuery = true)
    List<Object[]> getHourlyWaterLevelStats(String deviceId, LocalDateTime startTime);
}
