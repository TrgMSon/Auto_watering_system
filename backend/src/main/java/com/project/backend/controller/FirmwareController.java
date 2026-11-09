package com.project.backend.controller;

import org.springframework.core.io.ByteArrayResource;
import org.springframework.core.io.Resource;
import org.springframework.http.HttpHeaders;
import org.springframework.http.MediaType;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.firmware.FirmwareCheckResponse;
import com.project.backend.service.FirmwareService;

@RestController
@RequestMapping("/api/firmware")
public class FirmwareController {
    private final FirmwareService firmwareService;

    public FirmwareController(FirmwareService firmwareService) {
        this.firmwareService = firmwareService;
    }

    @GetMapping("/check")
    public ResponseEntity<ApiResponse<FirmwareCheckResponse>> checkForUpdate(
            @RequestParam(required = false) String currentVersion) {
        return ResponseEntity.ok(ApiResponse.success(firmwareService.checkForUpdate(currentVersion)));
    }

    @SuppressWarnings("null")
    @GetMapping("/{version}/download")
    public ResponseEntity<Resource> downloadFirmware(@PathVariable String version) {
        byte[] firmwareData = firmwareService.getFirmwareData(version);
        return ResponseEntity.ok()
                .contentType(MediaType.APPLICATION_OCTET_STREAM)
                .contentLength(firmwareData.length)
                .header(HttpHeaders.CONTENT_DISPOSITION,
                        "attachment; filename=\"firmware-" + version + ".bin\"")
                .body(new ByteArrayResource(firmwareData));
    }
}
