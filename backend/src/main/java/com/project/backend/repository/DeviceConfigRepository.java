package com.project.backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.DeviceConfig;

public interface DeviceConfigRepository extends JpaRepository<DeviceConfig, String> {
}
