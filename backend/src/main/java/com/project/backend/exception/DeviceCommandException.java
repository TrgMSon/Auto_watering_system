package com.project.backend.exception;

public class DeviceCommandException extends RuntimeException {
    public DeviceCommandException(String message) {
        super(message);
    }

    public DeviceCommandException(String message, Throwable cause) {
        super(message, cause);
    }
}
