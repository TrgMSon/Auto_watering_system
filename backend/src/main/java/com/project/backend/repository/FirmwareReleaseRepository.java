package com.project.backend.repository;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.FirmwareRelease;

public interface FirmwareReleaseRepository extends JpaRepository<FirmwareRelease, Long> {
    List<FirmwareRelease> findAllByOrderByCreatedAtDescIdDesc();

    Optional<FirmwareRelease> findFirstByOrderByCreatedAtDescIdDesc();

    Optional<FirmwareRelease> findByVersion(String version);

    boolean existsByVersion(String version);
}
