package com.project.backend.model.enums;

import java.util.Arrays;
import java.util.Locale;
import java.util.Optional;

public enum DeviceStatus {
    REGISTERED("registered"),
    PROVISIONED("provisioned"),
    ACTIVE("active"),
    OFFLINE("offline"),
    FAULT("fault"),
    MAINTENANCE("maintenance"),
    DECOMMISSIONED("decommissioned");

    private final String value;

    DeviceStatus(String value) {
        this.value = value;
    }

    public String value() {
        return value;
    }

    public static Optional<DeviceStatus> from(String value) {
        if (value == null) {
            return Optional.empty();
        }
        String normalized = value.trim().toLowerCase(Locale.ROOT);
        return Arrays.stream(values())
                .filter(status -> status.value.equals(normalized))
                .findFirst();
    }
}
