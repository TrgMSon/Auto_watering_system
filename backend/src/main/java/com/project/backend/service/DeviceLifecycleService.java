package com.project.backend.service;

public interface DeviceLifecycleService {
    void handleMqttStatus(String topic, String payload);

    boolean recordTelemetry(String deviceId);

    void markStaleDevicesOffline();

    void reportBleStatus(String deviceId, int userId, boolean reachable);

    void startMaintenance(String deviceId);

    void startMaintenance(String deviceId, int ownerId);

    void completeMaintenance(String deviceId);

    void completeMaintenance(String deviceId, int ownerId);

    void decommission(String deviceId);

    void setViewerLocked(String deviceId, int ownerId, int viewerId, boolean locked);
}
