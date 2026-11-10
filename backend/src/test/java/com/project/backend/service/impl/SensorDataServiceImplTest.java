package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.List;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.UserDevice;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.SensorLogRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.service.DeviceLifecycleService;

@ExtendWith(MockitoExtension.class)
class SensorDataServiceImplTest {
    @Mock
    private SensorLogRepository sensorLogRepository;
    @Mock
    private DeviceRepository deviceRepository;
    @Mock
    private UserDeviceRepository userDeviceRepository;
    @Mock
    private DeviceLifecycleService deviceLifecycleService;

    private SensorDataServiceImpl sensorDataService;

    @BeforeEach
    void setUp() {
        sensorDataService = new SensorDataServiceImpl(
                sensorLogRepository,
                deviceRepository,
                userDeviceRepository,
                new ObjectMapper(),
                deviceLifecycleService);
    }

    @ParameterizedTest
    @ValueSource(strings = { "owner", "viewer" })
    void allowsOwnerAndViewerToReadEnvironmentalStatistics(String permission) {
        UserDevice association = new UserDevice();
        association.setPermission(permission);
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "device-1"))
                .thenReturn(Optional.of(association));
        when(sensorLogRepository.getEnvironmentalStats(
                org.mockito.ArgumentMatchers.eq("device-1"),
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.anyInt()))
                .thenReturn(List.of());

        assertEquals(List.of(), sensorDataService.getEnvironmentalStats("device-1", 12, "24h"));

        verify(sensorLogRepository).getEnvironmentalStats(
                org.mockito.ArgumentMatchers.eq("device-1"),
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.eq(5));
    }

    @Test
    void deniesStatisticsWhenUserHasNoDeviceAssociation() {
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "device-1"))
                .thenReturn(Optional.empty());

        assertThrows(
                NotFoundException.class,
                () -> sensorDataService.getWaterLevelStats("device-1", 12, "24h"));

        verify(sensorLogRepository, never()).getWaterLevelStats(
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.anyInt());
    }

    @Test
    void deniesStatisticsForLockedViewer() {
        UserDevice association = new UserDevice();
        association.setPermission("viewer_locked");
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "device-1"))
                .thenReturn(Optional.of(association));

        assertThrows(
                NotFoundException.class,
                () -> sensorDataService.getEnvironmentalStats("device-1", 12, "24h"));

        verify(sensorLogRepository, never()).getEnvironmentalStats(
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.any(),
                org.mockito.ArgumentMatchers.anyInt());
    }
}
