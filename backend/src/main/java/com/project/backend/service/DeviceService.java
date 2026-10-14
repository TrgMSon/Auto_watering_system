package com.project.backend.service;

import java.util.List;

import com.project.backend.dto.device.DeviceCommandRequest;
import com.project.backend.dto.device.DeviceCommandResponse;
import com.project.backend.dto.device.CommandAcknowledgmentResponse;
import com.project.backend.dto.device.DeviceConfigRequest;
import com.project.backend.dto.device.DeviceConfigResponse;
import com.project.backend.dto.device.DeviceModeRequest;
import com.project.backend.dto.device.ScheduleRequest;
import com.project.backend.dto.device.ScheduleResponse;

public interface DeviceService {
    void registerDevice(String deviceId, int userId);

    DeviceConfigResponse getDeviceConfig(String deviceId, int userId);

    DeviceConfigResponse updateDeviceConfig(String deviceId, int userId, DeviceConfigRequest request);

    DeviceCommandResponse controlPump(String deviceId, int userId, DeviceCommandRequest request);

    CommandAcknowledgmentResponse getCommandAcknowledgment(
            String deviceId, String commandId, int userId);

    DeviceConfigResponse setWateringMode(String deviceId, int userId, DeviceModeRequest request);

    List<ScheduleResponse> getSchedules(String deviceId, int userId);

    ScheduleResponse createSchedule(String deviceId, int userId, ScheduleRequest request);

    ScheduleResponse updateSchedule(String deviceId, int scheduleId, int userId, ScheduleRequest request);

    void deleteSchedule(String deviceId, int scheduleId, int userId);
}
