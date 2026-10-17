package com.project.backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.UserDevice;

public interface UserDeviceRepository extends JpaRepository<UserDevice, Integer> {
    boolean existsByUser_IdAndDevice_Id(int userId, String deviceId);
}
