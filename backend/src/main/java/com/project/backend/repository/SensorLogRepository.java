package com.project.backend.repository;

import java.time.LocalDateTime;
import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.SensorLog;

public interface SensorLogRepository extends JpaRepository<SensorLog, Integer> {
    List<SensorLog> findByDevice_IdAndRecordedAtBetweenOrderByRecordedAtAsc(
            String deviceId, LocalDateTime from, LocalDateTime to);

    List<SensorLog> findTop1ByDevice_IdOrderByRecordedAtDesc(String deviceId);

    @Query(value = """
            SELECT
                CASE WHEN ?3 = 1440
                    THEN DATE_FORMAT(s.recorded_at, '%Y-%m-%d 00:00:00')
                    ELSE DATE_FORMAT(s.recorded_at - INTERVAL (MINUTE(s.recorded_at) MOD ?3) MINUTE,
                        '%Y-%m-%d %H:%i:00')
                END AS time_label,
                AVG(s.soil_moisture) AS avg_soil,
                AVG(s.temperature) AS avg_temp,
                AVG(s.humidity) AS avg_humid
            FROM sensor_log s
            WHERE s.device_id = ?1
              AND s.recorded_at >= ?2
              AND (s.soil_moisture IS NOT NULL OR s.temperature IS NOT NULL OR s.humidity IS NOT NULL)
            GROUP BY CASE WHEN ?3 = 1440
                THEN DATE_FORMAT(s.recorded_at, '%Y-%m-%d 00:00:00')
                ELSE DATE_FORMAT(s.recorded_at - INTERVAL (MINUTE(s.recorded_at) MOD ?3) MINUTE,
                    '%Y-%m-%d %H:%i:00')
            END
            ORDER BY time_label ASC
            """, nativeQuery = true)
    List<Object[]> getEnvironmentalStats(String deviceId, LocalDateTime startTime, int bucketMinutes);

    @Query(value = """
            SELECT
                CASE WHEN ?3 = 1440
                    THEN DATE_FORMAT(s.recorded_at, '%Y-%m-%d 00:00:00')
                    ELSE DATE_FORMAT(s.recorded_at - INTERVAL (MINUTE(s.recorded_at) MOD ?3) MINUTE,
                        '%Y-%m-%d %H:%i:00')
                END AS time_label,
                AVG(s.water_level) AS avg_water_level
            FROM sensor_log s
            WHERE s.device_id = ?1
              AND s.recorded_at >= ?2
              AND s.water_level IS NOT NULL
            GROUP BY CASE WHEN ?3 = 1440
                THEN DATE_FORMAT(s.recorded_at, '%Y-%m-%d 00:00:00')
                ELSE DATE_FORMAT(s.recorded_at - INTERVAL (MINUTE(s.recorded_at) MOD ?3) MINUTE,
                    '%Y-%m-%d %H:%i:00')
            END
            ORDER BY time_label ASC
            """, nativeQuery = true)
    List<Object[]> getWaterLevelStats(String deviceId, LocalDateTime startTime, int bucketMinutes);
}
