package com.project.backend.dto.device;

public record DeviceViewerResponse(
        int viewerId,
        String email,
        String permission) {
}
