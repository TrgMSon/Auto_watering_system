package com.project.backend.repository;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;
import org.springframework.data.repository.query.Param;
import org.springframework.transaction.annotation.Transactional;

import com.project.backend.model.UserDevice;

public interface UserDeviceRepository extends JpaRepository<UserDevice, Integer> {
    boolean existsByUser_IdAndDevice_Id(int userId, String deviceId);

    Optional<UserDevice> findByUser_IdAndDevice_Id(int userId, String deviceId);

    boolean existsByDevice_IdAndPermissionIgnoreCase(String deviceId, String permission);

    @Modifying
    @Transactional
    @Query(value = """
            UPDATE user_device
            SET permission = :permission
            WHERE user_id = :userId
              AND device_id = :deviceId
              AND LOWER(permission) IN ('viewer', 'viewer_locked')
            """, nativeQuery = true)
    int updateViewerPermission(
            @Param("userId") int userId,
            @Param("deviceId") String deviceId,
            @Param("permission") String permission);

    @Modifying
    @Transactional
    @Query(value = """
            DELETE FROM user_device
            WHERE user_id = :userId
              AND device_id = :deviceId
              AND LOWER(permission) IN ('viewer', 'viewer_locked')
            """, nativeQuery = true)
    int deleteViewerLink(@Param("userId") int userId, @Param("deviceId") String deviceId);

    @Transactional
    void deleteAllByDevice_Id(String deviceId);

    @Query("""
            SELECT userDevice FROM UserDevice userDevice
            JOIN FETCH userDevice.device
            WHERE userDevice.user.id = ?1
            """)
    List<UserDevice> findDevicesByUserId(int userId);

    @Query(value = """
            SELECT user_id FROM user_device WHERE device_id = ?1
            """, nativeQuery = true)
    List<String> findUserIds(String deviceId);
}
