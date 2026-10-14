package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.time.LocalDateTime;
import java.util.Optional;

import org.junit.jupiter.api.Test;

import com.project.backend.dto.device.DeviceConfigRequest;
import com.project.backend.dto.device.DeviceConfigResponse;
import com.project.backend.model.Device;
import com.project.backend.model.DeviceConfig;
import com.project.backend.repository.DeviceConfigRepository;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.ScheduleRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.repository.UserRepository;
import com.project.backend.service.MqttCommandAcknowledgmentService;

class DeviceServiceImplTests {
    @Test
    void savesReturnsAndPublishesPerDeviceWaterLevelThreshold() {
        DeviceRepository deviceRepository = mock(DeviceRepository.class);
        DeviceConfigRepository configRepository = mock(DeviceConfigRepository.class);
        ScheduleRepository scheduleRepository = mock(ScheduleRepository.class);
        UserDeviceRepository userDeviceRepository = mock(UserDeviceRepository.class);
        UserRepository userRepository = mock(UserRepository.class);
        MqttDeviceCommandPublisher commandPublisher = mock(MqttDeviceCommandPublisher.class);
        MqttCommandAcknowledgmentService acknowledgmentService =
                mock(MqttCommandAcknowledgmentService.class);

        String deviceId = "A4CF128FC130";
        when(deviceRepository.findById(deviceId))
                .thenReturn(Optional.of(new Device(
                        deviceId, "Garden", "v1", "v1", "active",
                        LocalDateTime.now(), LocalDateTime.now(), LocalDateTime.now())));
        when(userDeviceRepository.existsByUser_IdAndDevice_Id(7, deviceId)).thenReturn(true);
        when(configRepository.findById(deviceId)).thenReturn(Optional.empty());
        when(configRepository.save(any(DeviceConfig.class)))
                .thenAnswer(invocation -> invocation.getArgument(0));

        DeviceServiceImpl service = new DeviceServiceImpl(
                deviceRepository,
                configRepository,
                scheduleRepository,
                userDeviceRepository,
                userRepository,
                commandPublisher,
                acknowledgmentService);

        DeviceConfigRequest request = new DeviceConfigRequest();
        request.setAutoMode(true);
        request.setSoilMoistureMin(30.0f);
        request.setSoilMoistureMax(70.0f);
        request.setMaxWateringDuration(10);
        request.setAirHumidityMin(30.0f);
        request.setAirHumidityMax(80.0f);
        request.setAirTemperatureMin(10.0f);
        request.setAirTemperatureMax(40.0f);
        request.setWaterLevelMin(8);

        DeviceConfigResponse response = service.updateDeviceConfig(deviceId, 7, request);

        assertEquals(8, response.waterLevelMin());
        verify(commandPublisher).publishConfiguration(eq(deviceId), argThat(
                payload -> Integer.valueOf(8).equals(payload.get("min_water_level_alert"))));
    }
}
