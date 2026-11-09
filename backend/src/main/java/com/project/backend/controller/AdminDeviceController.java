package com.project.backend.controller;

import java.util.List;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import jakarta.validation.Valid;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.device.AdminDeviceResponse;
import com.project.backend.dto.device.CreateDeviceRequest;
import com.project.backend.dto.device.DeviceCreatedResponse;
import com.project.backend.service.DeviceLifecycleService;
import com.project.backend.service.DeviceService;

@RestController
@RequestMapping("/api/admin/devices")
public class AdminDeviceController {
    private final DeviceService deviceService;
    private final DeviceLifecycleService deviceLifecycleService;

    public AdminDeviceController(
            DeviceService deviceService,
            DeviceLifecycleService deviceLifecycleService) {
        this.deviceService = deviceService;
        this.deviceLifecycleService = deviceLifecycleService;
    }

    @GetMapping
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<List<AdminDeviceResponse>>> getAllDevices() {
        return ResponseEntity.ok(ApiResponse.success(deviceService.getAllDevices()));
    }

    @PostMapping
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<DeviceCreatedResponse>> createDevice(
            @Valid @RequestBody CreateDeviceRequest request) {
        DeviceCreatedResponse created = deviceService.createDevice(request);
        ApiResponse<DeviceCreatedResponse> response = ApiResponse.<DeviceCreatedResponse>builder()
                .success(true)
                .statusCode(HttpStatus.CREATED.value())
                .message("Thêm thiết bị thành công; thiết bị đang chờ gán chủ sở hữu")
                .data(created)
                .build();
        return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }

    @PostMapping("/{deviceId}/maintenance")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> startMaintenance(@PathVariable String deviceId) {
        deviceLifecycleService.startMaintenance(deviceId);
        return ResponseEntity.ok(ApiResponse.success("Đã chuyển thiết bị sang trạng thái bảo trì", null));
    }

    @PostMapping("/{deviceId}/maintenance/complete")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> completeMaintenance(@PathVariable String deviceId) {
        deviceLifecycleService.completeMaintenance(deviceId);
        return ResponseEntity.ok(ApiResponse.success("Đã hoàn tất bảo trì thiết bị", null));
    }

    @PostMapping("/{deviceId}/decommission")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> decommission(@PathVariable String deviceId) {
        deviceLifecycleService.decommission(deviceId);
        return ResponseEntity.ok(ApiResponse.success(
                "Quản trị viên đã thu hồi quyền sử dụng thiết bị, ngừng thiết bị và gỡ các liên kết người dùng", null));
    }
}
