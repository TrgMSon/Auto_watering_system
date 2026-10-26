package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.time.LocalDateTime;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import com.project.backend.dto.device.CreateDeviceRequest;
import com.project.backend.exception.ConflictException;
import com.project.backend.model.Device;
import com.project.backend.model.User;
import com.project.backend.model.enums.Role;
import com.project.backend.repository.DeviceConfigRepository;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.ScheduleRepository;
import com.project.backend.repository.SensorLogRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.repository.UserRepository;
import com.project.backend.service.MqttCommandAcknowledgmentService;

@ExtendWith(MockitoExtension.class)
class DeviceServiceImplTest {
    @Mock
    private DeviceRepository deviceRepository;
    @Mock
    private DeviceConfigRepository deviceConfigRepository;
    @Mock
    private ScheduleRepository scheduleRepository;
    @Mock
    private SensorLogRepository sensorLogRepository;
    @Mock
    private UserDeviceRepository userDeviceRepository;
    @Mock
    private UserRepository userRepository;
    @Mock
    private MqttDeviceCommandPublisher commandPublisher;
    @Mock
    private MqttCommandAcknowledgmentService acknowledgmentService;

    private DeviceServiceImpl deviceService;

    @BeforeEach
    void setUp() {
        deviceService = new DeviceServiceImpl(
                deviceRepository,
                deviceConfigRepository,
                scheduleRepository,
                sensorLogRepository,
                userDeviceRepository,
                userRepository,
                commandPublisher,
                acknowledgmentService);
    }

    @Test
    void createsDeviceWithInactiveStatusAndGeneratedBlePassKey() {
        CreateDeviceRequest request = new CreateDeviceRequest();
        request.setId("aa:bb:cc:dd:ee:ff");
        request.setDeviceName("  Garden pump  ");
        request.setHardwareVersion("  ESP32  ");
        request.setFirmwareVersion("  1.0.0  ");

        when(deviceRepository.save(any(Device.class)))
                .thenAnswer(invocation -> invocation.getArgument(0));

        var response = deviceService.createDevice(request);

        ArgumentCaptor<Device> deviceCaptor = ArgumentCaptor.forClass(Device.class);
        verify(deviceRepository).save(deviceCaptor.capture());
        Device savedDevice = deviceCaptor.getValue();

        assertEquals("AA:BB:CC:DD:EE:FF", savedDevice.getId());
        assertEquals("Garden pump", savedDevice.getDeviceName());
        assertEquals("ESP32", savedDevice.getHardwareVersion());
        assertEquals("1.0.0", savedDevice.getFirmwareVersion());
        assertEquals("inactive", savedDevice.getStatus());
        assertNotNull(savedDevice.getBlePassKey());
        assertTrue(savedDevice.getBlePassKey().length() >= 40);
        assertEquals("dev_AABBCCDDEEFF", savedDevice.getMqttUsername());
        assertNotNull(savedDevice.getMqttPassword());
        assertTrue(savedDevice.getMqttPassword().length() <= 55);
        assertNotNull(savedDevice.getCreatedAt());
        assertEquals(savedDevice.getCreatedAt(), savedDevice.getUpdatedAt());
        assertEquals("inactive", response.status());
        assertEquals(savedDevice.getId(), response.id());
        assertEquals(savedDevice.getBlePassKey(), response.bleSecretKey());
        assertEquals(savedDevice.getMqttUsername(), response.mqttUsername());
        assertEquals(savedDevice.getMqttPassword(), response.mqttPassword());
    }

    @Test
    void rejectsDuplicateDeviceId() {
        CreateDeviceRequest request = new CreateDeviceRequest();
        request.setId("AA:BB:CC:DD:EE:FF");
        when(deviceRepository.existsById(request.getId())).thenReturn(true);

        assertThrows(ConflictException.class, () -> deviceService.createDevice(request));
        verify(deviceRepository, never()).save(any(Device.class));
    }

    @Test
    void assigningInactiveDeviceActivatesItAndGrantsOwnerPermission() {
        String deviceId = "AA:BB:CC:DD:EE:FF";
        Device device = new Device();
        device.setId(deviceId);
        device.setStatus("inactive");
        User owner = new User();
        owner.setRole(Role.USER);

        when(deviceRepository.findById(deviceId)).thenReturn(Optional.of(device));
        when(userRepository.findById(12)).thenReturn(Optional.of(owner));
        when(deviceRepository.updateDeviceStatus("active", deviceId)).thenReturn(1);

        deviceService.registerDevice(deviceId, 12, 1);

        verify(deviceRepository).updateDeviceStatus("active", deviceId);
        verify(deviceRepository).addUserDevice(
                eq(12),
                eq(deviceId),
                eq("owner"),
                any(LocalDateTime.class));
    }
}
