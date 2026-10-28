package com.project.backend.dto.device;

import jakarta.validation.constraints.NotNull;

public record BleStatusRequest(@NotNull Boolean reachable) {
}
