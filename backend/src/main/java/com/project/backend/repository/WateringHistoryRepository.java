package com.project.backend.repository;

import java.time.LocalDateTime;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.WateringHistory;

import jakarta.transaction.Transactional;

public interface WateringHistoryRepository extends JpaRepository<WateringHistory, Integer> {
    @Query(value = """
        SELECT * FROM watering_history WHERE device_id = ?1 
        ORDER BY start_time DESC
        LIMIT 1
        """, nativeQuery = true)
    WateringHistory findStartWateringRecord(String deviceId);

    @Modifying 
    @Transactional 
    @Query(value = """
        UPDATE watering_history SET end_time = ?1 AND status = ?2 WHERE id = ?3
        """, nativeQuery = true)
    void updateEndWatering(LocalDateTime endTime, String status, int recordId);
}
