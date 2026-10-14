package com.project.backend.controller;

import java.util.List;

import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.DeleteMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.PutMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.device.DeviceCommandRequest;
import com.project.backend.dto.device.DeviceCommandResponse;
import com.project.backend.dto.device.DeviceConfigRequest;
import com.project.backend.dto.device.DeviceConfigResponse;
import com.project.backend.dto.device.DeviceModeRequest;
import com.project.backend.dto.device.ScheduleRequest;
import com.project.backend.dto.device.ScheduleResponse;
import com.project.backend.service.DeviceService;

import jakarta.validation.Valid;

@RestController
@RequestMapping("/api/devices/{id}")
public class DeviceController {
    private final DeviceService deviceService;

    public DeviceController(DeviceService deviceService) {
        this.deviceService = deviceService;
    }

    @GetMapping("/config")
    public ResponseEntity<ApiResponse<DeviceConfigResponse>> getDeviceConfig(
            @PathVariable String id, Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                deviceService.getDeviceConfig(id, userId(authentication))));
    }

    @PutMapping("/config")
    public ResponseEntity<ApiResponse<DeviceConfigResponse>> updateDeviceConfig(
            @PathVariable String id,
            @Valid @RequestBody DeviceConfigRequest request,
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                "Cập nhật cấu hình thiết bị thành công",
                deviceService.updateDeviceConfig(id, userId(authentication), request)));
    }

    @PostMapping("/control")
    public ResponseEntity<ApiResponse<DeviceCommandResponse>> controlPump(
            @PathVariable String id,
            @Valid @RequestBody DeviceCommandRequest request,
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                "Đã gửi lệnh điều khiển đến thiết bị",
                deviceService.controlPump(id, userId(authentication), request)));
    }

    @PostMapping("/mode")
    public ResponseEntity<ApiResponse<DeviceConfigResponse>> setModeWatering(
            @PathVariable String id,
            @Valid @RequestBody DeviceModeRequest request,
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                "Cập nhật chế độ tưới thành công",
                deviceService.setWateringMode(id, userId(authentication), request)));
    }

    @GetMapping("/schedules")
    public ResponseEntity<ApiResponse<List<ScheduleResponse>>> getSchedules(
            @PathVariable String id, Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                deviceService.getSchedules(id, userId(authentication))));
    }

    @PostMapping("/schedules")
    public ResponseEntity<ApiResponse<ScheduleResponse>> createSchedule(
            @PathVariable String id,
            @Valid @RequestBody ScheduleRequest request,
            Authentication authentication) {
            ScheduleResponse created = deviceService.createSchedule(id, userId(authentication), request);
            ApiResponse<ScheduleResponse> response = ApiResponse.<ScheduleResponse>builder()
                    .success(true)
                    .statusCode(HttpStatus.CREATED.value())
                    .message("Tạo lịch tưới thành công")
                    .data(created)
                    .build();
            return ResponseEntity.status(HttpStatus.CREATED).body(response);
    }

    @PutMapping("/schedules/{scheduleId}")
    public ResponseEntity<ApiResponse<ScheduleResponse>> updateSchedule(
            @PathVariable String id,
            @PathVariable int scheduleId,
            @Valid @RequestBody ScheduleRequest request,
            Authentication authentication) {
        return ResponseEntity.ok(ApiResponse.success(
                "Cập nhật lịch tưới thành công",
                deviceService.updateSchedule(id, scheduleId, userId(authentication), request)));
    }

    @DeleteMapping("/schedules/{scheduleId}")
    public ResponseEntity<ApiResponse<Void>> deleteSchedule(
            @PathVariable String id,
            @PathVariable int scheduleId,
            Authentication authentication) {
        deviceService.deleteSchedule(id, scheduleId, userId(authentication));
        return ResponseEntity.ok(ApiResponse.success("Xóa lịch tưới thành công", null));
    }

    @PostMapping("/register")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> registerDevice(
            @PathVariable("id") String deviceId, Authentication authentication) {
        deviceService.registerDevice(deviceId, userId(authentication));
        return ResponseEntity.ok(ApiResponse.success("Đăng ký thiết bị thành công", null));
    }

    private int userId(Authentication authentication) {
        return (Integer) authentication.getDetails();
    }
}
