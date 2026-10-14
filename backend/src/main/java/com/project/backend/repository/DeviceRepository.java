package com.project.backend.repository;

import java.time.LocalDateTime;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.Device;

import jakarta.transaction.Transactional;

public interface DeviceRepository extends JpaRepository<Device, String> {
    @Modifying
    @Transactional
    @Query(value = """
            UPDATE device SET status = ?1 WHERE id = ?2
            """, nativeQuery = true)
    int updateDeviceStatus(String status, String deviceId);

    @Modifying
    @Transactional
    @Query(value = """
            INSERT INTO user_device(user_id, device_id, permission, created_at) VALUES(?1, ?2, ?3, ?4)
            """, nativeQuery = true)
    void addUserDevice(int userId, String deviceId, String permission, LocalDateTime createdAt);
}
