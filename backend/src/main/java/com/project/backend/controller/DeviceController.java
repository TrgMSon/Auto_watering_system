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
import com.project.backend.dto.device.CommandAcknowledgmentResponse;
import com.project.backend.dto.device.BleStatusRequest;
import com.project.backend.dto.device.DeviceConfigRequest;
import com.project.backend.dto.device.DeviceConfigResponse;
import com.project.backend.dto.device.DeviceModeRequest;
import com.project.backend.dto.device.PumpControlRequest;
import com.project.backend.dto.device.RegisterDeviceRequest;
import com.project.backend.dto.device.ScheduleRequest;
import com.project.backend.dto.device.ScheduleResponse;
import com.project.backend.service.DeviceService;
import com.project.backend.service.DeviceLifecycleService;

import jakarta.validation.Valid;

@RestController
@RequestMapping("/api/devices/{id}")
public class DeviceController {
        private final DeviceService deviceService;
        private final DeviceLifecycleService deviceLifecycleService;

        public DeviceController(
                        DeviceService deviceService,
                        DeviceLifecycleService deviceLifecycleService) {
                this.deviceService = deviceService;
                this.deviceLifecycleService = deviceLifecycleService;
        }

        @PostMapping("/ble-status")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<Void>> reportBleStatus(
                        @PathVariable("id") String deviceId,
                        @Valid @RequestBody BleStatusRequest request,
                        Authentication authentication) {
                deviceLifecycleService.reportBleStatus(
                                deviceId, userId(authentication), request.reachable());
                return ResponseEntity.ok(ApiResponse.success("Đã cập nhật trạng thái kiểm tra BLE", null));
        }

        @PostMapping("/viewers/{viewerId}/lock")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<Void>> lockViewer(
                        @PathVariable("id") String deviceId,
                        @PathVariable int viewerId,
                        Authentication authentication) {
                deviceLifecycleService.setViewerLocked(
                                deviceId, userId(authentication), viewerId, true);
                return ResponseEntity.ok(ApiResponse.success("Đã tạm khóa quyền xem thiết bị", null));
        }

        @DeleteMapping("/viewers/{viewerId}/lock")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<Void>> unlockViewer(
                        @PathVariable("id") String deviceId,
                        @PathVariable int viewerId,
                        Authentication authentication) {
                deviceLifecycleService.setViewerLocked(
                                deviceId, userId(authentication), viewerId, false);
                return ResponseEntity.ok(ApiResponse.success("Đã mở khóa quyền xem thiết bị", null));
        }

        @PostMapping("/maintenance")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<Void>> startOwnerMaintenance(
                        @PathVariable("id") String deviceId,
                        Authentication authentication) {
                deviceLifecycleService.startMaintenance(deviceId, userId(authentication));
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã tạm ngưng điều khiển thiết bị để bảo trì", null));
        }

        @PostMapping("/maintenance/complete")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<Void>> completeOwnerMaintenance(
                        @PathVariable("id") String deviceId,
                        Authentication authentication) {
                deviceLifecycleService.completeMaintenance(deviceId, userId(authentication));
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã kết thúc chế độ bảo trì", null));
        }

        @GetMapping("/config")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceConfigResponse>> getDeviceConfig(
                        @PathVariable String id, Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                deviceService.getDeviceConfig(id, userId(authentication))));
        }

        @PostMapping("/control")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceCommandResponse>> controlPump(
                        @PathVariable("id") String id,
                        @Valid @RequestBody DeviceCommandRequest request,
                        Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã gửi lệnh, đang chờ thiết bị xác nhận",
                                deviceService.controlPump(id, userId(authentication), request)));
        }

        @GetMapping("/commands/{commandId}/ack")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<CommandAcknowledgmentResponse>> getCommandAcknowledgment(
                        @PathVariable("id") String id,
                        @PathVariable String commandId,
                        Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                deviceService.getCommandAcknowledgment(
                                                id, commandId, userId(authentication))));
        }

        @PostMapping("/mode")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceConfigResponse>> setModeWatering(
                        @PathVariable String id,
                        @Valid @RequestBody DeviceModeRequest request,
                        Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                "Cập nhật chế độ tưới thành công",
                                deviceService.setWateringMode(id, userId(authentication), request)));
        }

        @GetMapping("/schedules")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<List<ScheduleResponse>>> getSchedules(
                        @PathVariable String id, Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                deviceService.getSchedules(id, userId(authentication))));
        }

        @PostMapping("/schedules")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
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
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
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
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
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
                        @PathVariable("id") String deviceId, @Valid @RequestBody RegisterDeviceRequest request,
                        Authentication authentication) {
                deviceService.registerDevice(deviceId, request.getUserId(), userId(authentication));
                return ResponseEntity.ok(ApiResponse.success("Đăng ký thiết bị thành công", null));
        }

        // API điều khiển máy bơm (Mobile App gọi khi người dùng bấm nút Bật/Tắt bơm)
        @PostMapping("/pump")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceCommandResponse>> controlPump(
                        @PathVariable("id") String deviceId,
                        @Valid @RequestBody PumpControlRequest request,
                        Authentication authentication) {
                DeviceCommandRequest commandRequest = new DeviceCommandRequest();
                commandRequest.setAction(request.getState());
                commandRequest.setDurationSeconds(request.getDurationSeconds());
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã gửi lệnh, đang chờ thiết bị xác nhận",
                                deviceService.controlPump(
                                                deviceId, userId(authentication), commandRequest)));
        }

        // API cấu hình thông số cho thiết bị (Ngưỡng độ ẩm, thời gian tưới tự động,...)
        @PostMapping("/config")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceConfigResponse>> updateConfiguration(
                        @PathVariable("id") String deviceId,
                        @Valid @RequestBody DeviceConfigRequest request,
                        Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã cập nhật cấu hình, đang chờ thiết bị xác nhận",
                                deviceService.updateDeviceConfig(
                                                deviceId, userId(authentication), request)));
        }

        @PutMapping("/config")
        @PreAuthorize("hasAnyRole('USER', 'OWNER')")
        public ResponseEntity<ApiResponse<DeviceConfigResponse>> updateDeviceConfig(
                        @PathVariable("id") String deviceId,
                        @Valid @RequestBody DeviceConfigRequest request,
                        Authentication authentication) {
                return ResponseEntity.ok(ApiResponse.success(
                                "Đã cập nhật cấu hình, đang chờ thiết bị xác nhận",
                                deviceService.updateDeviceConfig(deviceId, userId(authentication), request)));
        }

        private int userId(Authentication authentication) {
                return (Integer) authentication.getDetails();
        }
}
