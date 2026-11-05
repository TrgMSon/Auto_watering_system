package com.project.backend.service;

import java.nio.file.Path;
import java.util.List;

import org.springframework.web.multipart.MultipartFile;

import com.project.backend.dto.firmware.FirmwareCheckResponse;
import com.project.backend.dto.firmware.FirmwareReleaseResponse;

public interface FirmwareService {
    List<FirmwareReleaseResponse> getReleases();

    FirmwareReleaseResponse upload(String version, String notes, MultipartFile file);

    FirmwareCheckResponse checkForUpdate(String currentVersion);

    Path getFirmwareFile(String version);
}
