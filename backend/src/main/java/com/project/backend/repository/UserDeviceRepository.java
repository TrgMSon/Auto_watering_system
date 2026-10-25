package com.project.backend.repository;

import java.util.List;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.UserDevice;

public interface UserDeviceRepository extends JpaRepository<UserDevice, Integer> {
    boolean existsByUser_IdAndDevice_Id(int userId, String deviceId);

    @Query(value = """
            SELECT user_id FROM user_device WHERE device_id = ?1
            """, nativeQuery = true)
    List<String> findUserIds(String deviceId);
}
