package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.util.Locale;
import java.util.Optional;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.dto.device.DeviceStatusMessage;
import com.project.backend.exception.ConflictException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.Device;
import com.project.backend.model.UserDevice;
import com.project.backend.model.enums.DeviceStatus;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.service.DeviceLifecycleService;
import com.project.backend.service.EmqxManagementService;

@Service
public class DeviceLifecycleServiceImpl implements DeviceLifecycleService {
    private static final Logger logger = LoggerFactory.getLogger(DeviceLifecycleServiceImpl.class);
    private static final long HEARTBEAT_TIMEOUT_SECONDS = 30;

    private final DeviceRepository deviceRepository;
    private final UserDeviceRepository userDeviceRepository;
    private final ObjectMapper objectMapper;
    private final EmqxManagementService emqxManagementService;

    public DeviceLifecycleServiceImpl(
            DeviceRepository deviceRepository,
            UserDeviceRepository userDeviceRepository,
            ObjectMapper objectMapper,
            EmqxManagementService emqxManagementService) {
        this.deviceRepository = deviceRepository;
        this.userDeviceRepository = userDeviceRepository;
        this.objectMapper = objectMapper;
        this.emqxManagementService = emqxManagementService;
    }

    @Override
    @Transactional
    public void handleMqttStatus(String topic, String payload) {
        String deviceId = deviceIdFromTopic(topic);
        if (deviceId == null) {
            logger.warn("Ignoring malformed device status topic: {}", topic);
            return;
        }

        DeviceStatusMessage message;
        try {
            message = objectMapper.readValue(payload, DeviceStatusMessage.class);
        } catch (JsonProcessingException exception) {
            logger.warn("Ignoring malformed device status payload for {}", deviceId, exception);
            return;
        }

        Optional<DeviceStatus> requestedStatus = DeviceStatus.from(message.status());
        if (requestedStatus.isEmpty()) {
            logger.warn("Ignoring unknown device status for {}: {}", deviceId, message.status());
            return;
        }

        Optional<Device> foundDevice = deviceRepository.findById(deviceId);
        if (foundDevice.isEmpty()) {
            logger.warn("Ignoring status update from unknown device {}", deviceId);
            return;
        }
        Device device = foundDevice.get();
        DeviceStatus current = lifecycleStatus(device);
        if (current == null || current == DeviceStatus.DECOMMISSIONED) {
            return;
        }

        LocalDateTime now = LocalDateTime.now();
        switch (requestedStatus.get()) {
            case ACTIVE -> activateFromDevice(device, current, message.firmwareVersion(), now);
            case OFFLINE -> {
                if (current == DeviceStatus.ACTIVE) {
                    device.setStatus(DeviceStatus.OFFLINE.value());
                    device.setUpdatedAt(now);
                    deviceRepository.save(device);
                }
            }
            case MAINTENANCE -> {
                if (current == DeviceStatus.ACTIVE || current == DeviceStatus.OFFLINE) {
                    device.setStatus(DeviceStatus.MAINTENANCE.value());
                    device.setUpdatedAt(now);
                    deviceRepository.save(device);
                }
            }
            default -> logger.warn("Ignoring device status transition {} -> {} for {}",
                    current.value(), requestedStatus.get().value(), deviceId);
        }
    }

    @Override
    @Transactional
    public boolean recordTelemetry(String deviceId) {
        Optional<Device> foundDevice = deviceRepository.findById(canonicalDeviceId(deviceId));
        if (foundDevice.isEmpty()) {
            logger.warn("Ignoring telemetry from unknown device {}", deviceId);
            return false;
        }
        Device device = foundDevice.get();
        DeviceStatus current = lifecycleStatus(device);
        if (current == null) {
            logger.warn("Ignoring telemetry from device {} with unknown state {}", deviceId, device.getStatus());
            return false;
        }
        if (current != DeviceStatus.PROVISIONED
                && current != DeviceStatus.ACTIVE
                && current != DeviceStatus.OFFLINE) {
            logger.warn("Ignoring telemetry from device {} in state {}", deviceId, current.value());
            return false;
        }
        activateFromDevice(device, current, null, LocalDateTime.now());
        return true;
    }

    @Override
    @Scheduled(fixedDelay = 10_000)
    @Transactional
    public void markStaleDevicesOffline() {
        LocalDateTime cutoff = LocalDateTime.now().minusSeconds(HEARTBEAT_TIMEOUT_SECONDS);
        for (Device device : deviceRepository.findAllByStatusAndLastActiveAtBefore(
                DeviceStatus.ACTIVE.value(), cutoff)) {
            device.setStatus(DeviceStatus.OFFLINE.value());
            device.setUpdatedAt(LocalDateTime.now());
            deviceRepository.save(device);
        }
    }

    @Override
    @Transactional
    public void reportBleStatus(String deviceId, int userId, boolean reachable) {
        Device device = requireDevice(deviceId);
        requireDevicePermission(device.getId(), userId);
        DeviceStatus current = requireLifecycleStatus(device);
        if (current == DeviceStatus.DECOMMISSIONED) {
            throw new ConflictException("Thiết bị đã ngừng hoạt động");
        }
        if (current != DeviceStatus.PROVISIONED
                && current != DeviceStatus.ACTIVE
                && current != DeviceStatus.OFFLINE
                && current != DeviceStatus.FAULT) {
            throw new ConflictException("Không thể cập nhật trạng thái BLE của thiết bị ở trạng thái hiện tại");
        }

        LocalDateTime now = LocalDateTime.now();
        if (!reachable) {
            device.setStatus(DeviceStatus.FAULT.value());
            device.setUpdatedAt(now);
            deviceRepository.save(device);
            return;
        }

        if (current == DeviceStatus.FAULT) {
            device.setStatus(isRecentlyActive(device, now)
                    ? DeviceStatus.ACTIVE.value()
                    : device.getLastActiveAt() == null
                            ? DeviceStatus.PROVISIONED.value()
                            : DeviceStatus.OFFLINE.value());
            device.setUpdatedAt(now);
            deviceRepository.save(device);
        }
    }

    @Override
    @Transactional
    public void startMaintenance(String deviceId) {
        startMaintenance(deviceId, null);
    }

    @Override
    @Transactional
    public void startMaintenance(String deviceId, int ownerId) {
        startMaintenance(deviceId, Integer.valueOf(ownerId));
    }

    private void startMaintenance(String deviceId, Integer ownerId) {
        Device device = requireDevice(deviceId);
        if (ownerId != null) {
            requireDevicePermission(device.getId(), ownerId);
        }
        DeviceStatus current = requireLifecycleStatus(device);
        if (current == DeviceStatus.DECOMMISSIONED) {
            throw new ConflictException("Thiết bị đã ngừng hoạt động");
        }
        if (current == DeviceStatus.REGISTERED || current == DeviceStatus.MAINTENANCE) {
            throw new ConflictException("Thiết bị chưa được gán chủ sở hữu hoặc đang bảo trì");
        }

        device.setStatus(DeviceStatus.MAINTENANCE.value());
        device.setUpdatedAt(LocalDateTime.now());
        deviceRepository.save(device);
    }

    @Override
    @Transactional
    public void completeMaintenance(String deviceId) {
        completeMaintenance(deviceId, null);
    }

    @Override
    @Transactional
    public void completeMaintenance(String deviceId, int ownerId) {
        completeMaintenance(deviceId, Integer.valueOf(ownerId));
    }

    private void completeMaintenance(String deviceId, Integer ownerId) {
        Device device = requireDevice(deviceId);
        if (ownerId != null) {
            requireDevicePermission(device.getId(), ownerId);
        }
        if (requireLifecycleStatus(device) != DeviceStatus.MAINTENANCE) {
            throw new ConflictException("Thiết bị không ở trạng thái bảo trì");
        }

        LocalDateTime now = LocalDateTime.now();
        device.setStatus(isRecentlyActive(device, now)
                ? DeviceStatus.ACTIVE.value()
                : userDeviceRepository.existsByDevice_IdAndPermissionIgnoreCase(
                        device.getId(), "owner")
                        ? DeviceStatus.PROVISIONED.value()
                        : DeviceStatus.REGISTERED.value());
        device.setUpdatedAt(now);
        deviceRepository.save(device);
    }

    @Override
    @Transactional
    public void decommission(String deviceId) {
        Device device = requireDevice(deviceId);
        if (requireLifecycleStatus(device) == DeviceStatus.DECOMMISSIONED) {
            throw new ConflictException("Thiết bị đã ngừng hoạt động");
        }

        emqxManagementService.deleteDeviceUser(device.getMqttUsername());
        device.setStatus(DeviceStatus.DECOMMISSIONED.value());
        device.setBlePassKey(null);
        device.setMqttUsername(null);
        device.setMqttPassword(null);
        device.setUpdatedAt(LocalDateTime.now());
        deviceRepository.save(device);
        userDeviceRepository.deleteAllByDevice_Id(device.getId());
    }

    @Override
    @Transactional
    public void setViewerLocked(String deviceId, int ownerId, int viewerId, boolean locked) {
        Device device = requireDevice(deviceId);
        requireOwnerPermission(device.getId(), ownerId);
        int updated = userDeviceRepository.updateViewerPermission(
                viewerId, device.getId(), locked ? "viewer_locked" : "viewer");
        if (updated != 1) {
            throw new NotFoundException("Không tìm thấy người xem của thiết bị");
        }
    }

    private void activateFromDevice(
            Device device, DeviceStatus current, String firmwareVersion, LocalDateTime now) {
        if (current != DeviceStatus.PROVISIONED
                && current != DeviceStatus.ACTIVE
                && current != DeviceStatus.OFFLINE) {
            return;
        }
        device.setStatus(DeviceStatus.ACTIVE.value());
        device.setLastActiveAt(now);
        device.setUpdatedAt(now);
        if (firmwareVersion != null && !firmwareVersion.isBlank()) {
            device.setFirmwareVersion(firmwareVersion.trim());
        }
        deviceRepository.save(device);
    }

    private boolean isRecentlyActive(Device device, LocalDateTime now) {
        return device.getLastActiveAt() != null
                && !device.getLastActiveAt().isBefore(now.minusSeconds(HEARTBEAT_TIMEOUT_SECONDS));
    }

    private void requireOwnerPermission(String deviceId, int userId) {
        UserDevice association = userDeviceRepository.findByUser_IdAndDevice_Id(userId, deviceId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));
        if (!"owner".equalsIgnoreCase(association.getPermission())) {
            throw new ConflictException("Chỉ chủ sở hữu mới có quyền quản lý thiết bị");
        }
    }

    private void requireDevicePermission(String deviceId, int userId) {
        UserDevice association = userDeviceRepository.findByUser_IdAndDevice_Id(userId, deviceId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));
        if ("viewer_locked".equalsIgnoreCase(association.getPermission())
                || !("owner".equalsIgnoreCase(association.getPermission())
                        || "viewer".equalsIgnoreCase(association.getPermission()))) {
            throw new NotFoundException("Không tìm thấy thiết bị");
        }
    }

    private Device requireDevice(String deviceId) {
        return deviceRepository.findById(canonicalDeviceId(deviceId))
                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));
    }

    private DeviceStatus requireLifecycleStatus(Device device) {
        DeviceStatus status = lifecycleStatus(device);
        if (status == null) {
            throw new ConflictException("Trạng thái thiết bị không hợp lệ");
        }
        return status;
    }

    private DeviceStatus lifecycleStatus(Device device) {
        if ("inactive".equalsIgnoreCase(device.getStatus())) {
            return DeviceStatus.REGISTERED;
        }
        return DeviceStatus.from(device.getStatus()).orElse(null);
    }

    private String deviceIdFromTopic(String topic) {
        if (topic == null || !topic.startsWith("iot/garden/") || !topic.endsWith("/status")) {
            return null;
        }
        String deviceId = topic.substring("iot/garden/".length(), topic.length() - "/status".length());
        if (deviceId.isBlank() || deviceId.contains("/")) {
            return null;
        }
        return canonicalDeviceId(deviceId);
    }

    private String canonicalDeviceId(String deviceId) {
        return deviceId == null ? null : deviceId.trim().replace('-', ':').toUpperCase(Locale.ROOT);
    }
}
