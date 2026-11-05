package com.project.backend.controller;

import java.util.List;

import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.multipart.MultipartFile;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.firmware.FirmwareReleaseResponse;
import com.project.backend.service.FirmwareService;

@RestController
@RequestMapping("/api/admin/firmware")
@PreAuthorize("hasRole('ADMIN')")
public class AdminFirmwareController {
    private final FirmwareService firmwareService;

    public AdminFirmwareController(FirmwareService firmwareService) {
        this.firmwareService = firmwareService;
    }

    @GetMapping
    public ResponseEntity<ApiResponse<List<FirmwareReleaseResponse>>> getReleases() {
        return ResponseEntity.ok(ApiResponse.success(firmwareService.getReleases()));
    }

    @PostMapping(consumes = MediaType.MULTIPART_FORM_DATA_VALUE)
    public ResponseEntity<ApiResponse<FirmwareReleaseResponse>> uploadFirmware(
            @RequestParam("version") String version,
            @RequestParam(value = "notes", required = false) String notes,
            @RequestParam("file") MultipartFile file) {
        FirmwareReleaseResponse release = firmwareService.upload(version, notes, file);
        ApiResponse<FirmwareReleaseResponse> response = ApiResponse.<FirmwareReleaseResponse>builder()
                .success(true)
                .statusCode(HttpStatus.CREATED.value())
                .message("Đã tải lên firmware và phát hành phiên bản mới")
                .data(release)
                .build();
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }
}
