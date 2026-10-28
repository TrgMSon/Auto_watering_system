package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertDoesNotThrow;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.inOrder;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.InOrder;
import org.mockito.junit.jupiter.MockitoExtension;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.model.Device;
import com.project.backend.model.UserDevice;
import com.project.backend.exception.DeviceCommandException;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.service.EmqxManagementService;

@ExtendWith(MockitoExtension.class)
class DeviceLifecycleServiceImplTest {
    @Mock
    private DeviceRepository deviceRepository;
    @Mock
    private UserDeviceRepository userDeviceRepository;
    @Mock
    private EmqxManagementService emqxManagementService;

    private DeviceLifecycleServiceImpl lifecycleService;

    @BeforeEach
    void setUp() {
        lifecycleService = new DeviceLifecycleServiceImpl(
                deviceRepository, userDeviceRepository, new ObjectMapper(), emqxManagementService);
    }

    @Test
    void activeStatusMessageActivatesProvisionedDeviceAndUpdatesHeartbeat() {
        Device device = device("provisioned");
        when(deviceRepository.findById(device.getId())).thenReturn(Optional.of(device));

        lifecycleService.handleMqttStatus(
                "iot/garden/" + device.getId() + "/status",
                "{\"status\":\"ACTIVE\",\"firmware_version\":\"1.2.0\"}");

        assertEquals("active", device.getStatus());
        assertEquals("1.2.0", device.getFirmwareVersion());
        org.junit.jupiter.api.Assertions.assertNotNull(device.getLastActiveAt());
        verify(deviceRepository).save(device);
    }

    @Test
    void lateHeartbeatMovesActiveDeviceOffline() {
        Device device = device("active");
        when(deviceRepository.findAllByStatusAndLastActiveAtBefore(
                eq("active"), any(LocalDateTime.class))).thenReturn(List.of(device));

        lifecycleService.markStaleDevicesOffline();

        assertEquals("offline", device.getStatus());
        verify(deviceRepository).save(device);
    }

    @Test
    void unreachableBleReportMarksOwnedDeviceFault() {
        Device device = device("offline");
        UserDevice association = new UserDevice();
        association.setPermission("owner");
        when(deviceRepository.findById(device.getId())).thenReturn(Optional.of(device));
        when(userDeviceRepository.findByUser_IdAndDevice_Id(27, device.getId()))
                .thenReturn(Optional.of(association));

        lifecycleService.reportBleStatus(device.getId(), 27, false);

        assertEquals("fault", device.getStatus());
        verify(deviceRepository).save(device);
    }

    @Test
    void viewerCanReportBleStatusForSharedDevice() {
        Device device = device("offline");
        UserDevice association = new UserDevice();
        association.setPermission("viewer");
        when(deviceRepository.findById(device.getId())).thenReturn(Optional.of(device));
        when(userDeviceRepository.findByUser_IdAndDevice_Id(27, device.getId()))
                .thenReturn(Optional.of(association));

        lifecycleService.reportBleStatus(device.getId(), 27, false);

        assertEquals("fault", device.getStatus());
        verify(deviceRepository).save(device);
    }

    @Test
    void decommissionClearsCredentialsAndRemovesDeviceAssociations() {
        Device device = device("active");
        device.setBlePassKey("ble-secret");
        device.setMqttUsername("device-user");
        device.setMqttPassword("mqtt-secret");
        when(deviceRepository.findById(device.getId())).thenReturn(Optional.of(device));

        lifecycleService.decommission(device.getId());

        assertEquals("decommissioned", device.getStatus());
        org.junit.jupiter.api.Assertions.assertNull(device.getBlePassKey());
        org.junit.jupiter.api.Assertions.assertNull(device.getMqttUsername());
        org.junit.jupiter.api.Assertions.assertNull(device.getMqttPassword());
        verify(deviceRepository).save(device);
        verify(userDeviceRepository).deleteAllByDevice_Id(device.getId());
        InOrder inOrder = inOrder(emqxManagementService, deviceRepository, userDeviceRepository);
        inOrder.verify(emqxManagementService).deleteDeviceUser("device-user");
        inOrder.verify(deviceRepository).save(device);
        inOrder.verify(userDeviceRepository).deleteAllByDevice_Id(device.getId());
    }

    @Test
    void failedBrokerRevocationLeavesDeviceAndAssociationsUnchanged() {
        Device device = device("active");
        device.setBlePassKey("ble-secret");
        device.setMqttUsername("device-user");
        device.setMqttPassword("mqtt-secret");
        when(deviceRepository.findById(device.getId())).thenReturn(Optional.of(device));
        org.mockito.Mockito.doThrow(new DeviceCommandException("EMQX unavailable"))
                .when(emqxManagementService).deleteDeviceUser("device-user");

        assertThrows(DeviceCommandException.class, () -> lifecycleService.decommission(device.getId()));

        assertEquals("active", device.getStatus());
        assertEquals("ble-secret", device.getBlePassKey());
        assertEquals("device-user", device.getMqttUsername());
        assertEquals("mqtt-secret", device.getMqttPassword());
        verify(deviceRepository, org.mockito.Mockito.never()).save(device);
        verify(userDeviceRepository, org.mockito.Mockito.never()).deleteAllByDevice_Id(device.getId());
    }

    @Test
    void ignoresMalformedStatusAndDoesNotMutateDevice() {
        assertDoesNotThrow(() -> lifecycleService.handleMqttStatus(
                "iot/garden/AA:BB:CC:DD:EE:FF/status", "not-json"));
    }

    private Device device(String status) {
        Device device = new Device();
        device.setId("AA:BB:CC:DD:EE:FF");
        device.setStatus(status);
        return device;
    }
}
