package com.project.backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.FirmwareBinary;

public interface FirmwareBinaryRepository extends JpaRepository<FirmwareBinary, Long> {
}
