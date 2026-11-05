package com.project.backend.dto.device;

public record DeviceSharedNotification(
        String type,
        String deviceId,
        String deviceName,
        String message) {
}
