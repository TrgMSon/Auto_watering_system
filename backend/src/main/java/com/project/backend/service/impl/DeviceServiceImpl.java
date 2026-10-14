package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import com.project.backend.dto.device.DeviceCommandRequest;
import com.project.backend.dto.device.DeviceCommandResponse;
import com.project.backend.dto.device.CommandAcknowledgmentResponse;
import com.project.backend.dto.device.DeviceConfigRequest;
import com.project.backend.dto.device.DeviceConfigResponse;
import com.project.backend.dto.device.DeviceModeRequest;
import com.project.backend.dto.device.ScheduleRequest;
import com.project.backend.dto.device.ScheduleResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.Device;
import com.project.backend.model.DeviceConfig;
import com.project.backend.model.Schedule;
import com.project.backend.model.User;
import com.project.backend.model.enums.Role;
import com.project.backend.repository.DeviceConfigRepository;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.ScheduleRepository;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.repository.UserRepository;
import com.project.backend.service.DeviceService;
import com.project.backend.service.MqttCommandAcknowledgmentService;

@Service
public class DeviceServiceImpl implements DeviceService {
    private final DeviceRepository deviceRepository;
    private final DeviceConfigRepository deviceConfigRepository;
    private final ScheduleRepository scheduleRepository;
    private final UserDeviceRepository userDeviceRepository;
    private final UserRepository userRepository;
    private final MqttDeviceCommandPublisher commandPublisher;
    private final MqttCommandAcknowledgmentService acknowledgmentService;

    public DeviceServiceImpl(
            DeviceRepository deviceRepository,
            DeviceConfigRepository deviceConfigRepository,
            ScheduleRepository scheduleRepository,
            UserDeviceRepository userDeviceRepository,
            UserRepository userRepository,
            MqttDeviceCommandPublisher commandPublisher,
            MqttCommandAcknowledgmentService acknowledgmentService) {
        this.deviceRepository = deviceRepository;
        this.deviceConfigRepository = deviceConfigRepository;
        this.scheduleRepository = scheduleRepository;
        this.userDeviceRepository = userDeviceRepository;
        this.userRepository = userRepository;
        this.commandPublisher = commandPublisher;
        this.acknowledgmentService = acknowledgmentService;
    }

    @Override
    @Transactional
    public void registerDevice(String deviceId, int userId, int userCreateId) {
        Device device = deviceRepository.findById(deviceId)
                .orElseThrow(() -> new NotFoundException("Thiết bị không tồn tại"));
        User user = userRepository.findById(userId)
                .orElseThrow(() -> new NotFoundException("Người dùng không tồn tại"));

        if (user.getRole() != Role.USER) {
            throw new ConflictException("Người dùng có vai trò không hợp lệ");
        }
        if (userDeviceRepository.existsByUser_IdAndDevice_Id(userId, deviceId)) {
            throw new ConflictException("Thiết bị đã được đăng ký với tài khoản này");
        }
        if (device.getStatus() != null && device.getStatus().equalsIgnoreCase("active")) {
            throw new ConflictException("Thiết bị đã được đăng ký");
        }

        User userCreate = userRepository.findById(userId)
                .orElseThrow(() -> new NotFoundException("Người dùng đăng ký thiết bị không tồn tại"));
        if (userCreate.getRole() == Role.ADMIN)
            userRepository.assignOwner("owner", userId);

        if (deviceRepository.updateDeviceStatus("active", deviceId) != 1) {
            throw new ConflictException("Không thể cập nhật trạng thái thiết bị");
        }
        deviceRepository.addUserDevice(userId, deviceId, "owner", LocalDateTime.now());
    }

    @Override
    @Transactional(readOnly = true)
    public DeviceConfigResponse getDeviceConfig(String deviceId, int userId) {
        requireOwnedDevice(deviceId, userId);
        DeviceConfig config = deviceConfigRepository.findById(deviceId)
                .orElseThrow(() -> new NotFoundException("Chưa có cấu hình cho thiết bị này"));
        return toConfigResponse(config);
    }

    @Override
    @Transactional
    public DeviceConfigResponse updateDeviceConfig(
            String deviceId, int userId, DeviceConfigRequest request) {
        Device device = requireOwnedDevice(deviceId, userId);
        validateThresholds(
                request.getSoilMoistureMin(), request.getSoilMoistureMax(),
                "Độ ẩm đất tối thiểu phải nhỏ hơn hoặc bằng tối đa");
        validateThresholds(
                request.getAirHumidityMin(), request.getAirHumidityMax(),
                "Độ ẩm không khí tối thiểu phải nhỏ hơn hoặc bằng tối đa");
        validateThresholds(
                request.getAirTemperatureMin(), request.getAirTemperatureMax(),
                "Nhiệt độ tối thiểu phải nhỏ hơn hoặc bằng tối đa");

        DeviceConfig config = deviceConfigRepository.findById(deviceId)
                .orElseGet(() -> new DeviceConfig(deviceId, 0, 0, 0, 1,
                        LocalDateTime.now(), 0, 0, 0, 0, 0));
        config.setAutoMode(request.getAutoMode() ? 1 : 0);
        config.setSoilMoistureMin(request.getSoilMoistureMin());
        config.setSoilMoistureMax(request.getSoilMoistureMax());
        config.setMaxWateringDuration(request.getMaxWateringDuration());
        config.setAirHumidityMin(request.getAirHumidityMin());
        config.setAirHumidityMax(request.getAirHumidityMax());
        config.setAirTemperatureMin(request.getAirTemperatureMin());
        config.setAirTemperatureMax(request.getAirTemperatureMax());
        config.setWaterLevelMin(request.getWaterLevelMin());
        config.setUpdatedAt(LocalDateTime.now());

        DeviceConfig savedConfig = deviceConfigRepository.save(config);
        String commandId = commandPublisher.publishConfiguration(deviceId, configPayload(savedConfig));
        return toConfigResponse(savedConfig, commandId);
    }

    @Override
    @Transactional
    public DeviceCommandResponse controlPump(
            String deviceId, int userId, DeviceCommandRequest request) {
        requireOwnedDevice(deviceId, userId);
        String action = request.getAction().trim().toUpperCase(Locale.ROOT);
        if (!action.equals("ON") && !action.equals("OFF")) {
            throw new IllegalArgumentException("action chỉ nhận giá trị ON hoặc OFF");
        }

        int durationSeconds = 0;
        if (action.equals("ON")) {
            int maxDurationSeconds = deviceConfigRepository.findById(deviceId)
                    .map(config -> Math.multiplyExact(config.getMaxWateringDuration(), 60))
                    .orElse(600);
            durationSeconds = request.getDurationSeconds() == null
                    ? Math.min(300, maxDurationSeconds)
                    : request.getDurationSeconds();
            if (durationSeconds > maxDurationSeconds) {
                throw new IllegalArgumentException(
                        "duration_seconds không được vượt quá thời gian tưới tối đa đã cấu hình");
            }
        }
        String commandId = commandPublisher.publishPumpControl(deviceId, action, durationSeconds);
        return new DeviceCommandResponse(deviceId, action, "PENDING", commandId);
    }

    @Override
    @Transactional(readOnly = true)
    public CommandAcknowledgmentResponse getCommandAcknowledgment(
            String deviceId, String commandId, int userId) {
        requireOwnedDevice(deviceId, userId);
        return acknowledgmentService.find(deviceId, commandId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy trạng thái xác nhận lệnh"));
    }

    @Override
    @Transactional
    public DeviceConfigResponse setWateringMode(
            String deviceId, int userId, DeviceModeRequest request) {
        requireOwnedDevice(deviceId, userId);
        String mode = request.getMode().trim().toLowerCase(Locale.ROOT);
        if (!mode.equals("auto") && !mode.equals("manual")) {
            throw new IllegalArgumentException("mode chỉ nhận giá trị auto hoặc manual");
        }

        DeviceConfig config = deviceConfigRepository.findById(deviceId)
                .orElseThrow(() -> new NotFoundException("Chưa có cấu hình cho thiết bị này"));
        config.setAutoMode(mode.equals("auto") ? 1 : 0);
        config.setUpdatedAt(LocalDateTime.now());
        DeviceConfig savedConfig = deviceConfigRepository.save(config);

        String commandId = commandPublisher.publishConfiguration(deviceId, configPayload(savedConfig));
        return toConfigResponse(savedConfig, commandId);
    }

    @Override
    @Transactional(readOnly = true)
    public List<ScheduleResponse> getSchedules(String deviceId, int userId) {
        requireOwnedDevice(deviceId, userId);
        return scheduleRepository.findAllByDevice_IdOrderByStartTimeAsc(deviceId)
                .stream()
                .map(this::toScheduleResponse)
                .toList();
    }

    @Override
    @Transactional
    public ScheduleResponse createSchedule(
            String deviceId, int userId, ScheduleRequest request) {
        Device device = requireOwnedDevice(deviceId, userId);
        Schedule schedule = new Schedule(
                0,
                device,
                request.getStartTime(),
                request.getEndTime(),
                request.getDurationMinutes(),
                request.getDaysOfWeek().trim(),
                request.isEnabled(),
                LocalDateTime.now(),
                LocalDateTime.now());
        return toScheduleResponse(scheduleRepository.save(schedule));
    }

    @Override
    @Transactional
    public ScheduleResponse updateSchedule(
            String deviceId, int scheduleId, int userId, ScheduleRequest request) {
        requireOwnedDevice(deviceId, userId);
        Schedule schedule = scheduleRepository.findByIdAndDevice_Id(scheduleId, deviceId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy lịch tưới"));
        schedule.setStartTime(request.getStartTime());
        schedule.setEndTime(request.getEndTime());
        schedule.setDurationMinutes(request.getDurationMinutes());
        schedule.setDaysOfWeek(request.getDaysOfWeek().trim());
        schedule.setEnabled(request.isEnabled());
        schedule.setUpdatedAt(LocalDateTime.now());
        return toScheduleResponse(scheduleRepository.save(schedule));
    }

    @Override
    @Transactional
    public void deleteSchedule(String deviceId, int scheduleId, int userId) {
        requireOwnedDevice(deviceId, userId);
        Schedule schedule = scheduleRepository.findByIdAndDevice_Id(scheduleId, deviceId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy lịch tưới"));
        scheduleRepository.delete(schedule);
    }

    private Device requireOwnedDevice(String deviceId, int userId) {
        Device device = deviceRepository.findById(deviceId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));
        if (!userDeviceRepository.existsByUser_IdAndDevice_Id(userId, deviceId)) {
            throw new NotFoundException("Không tìm thấy thiết bị");
        }
        return device;
    }

    private void validateThresholds(float minimum, float maximum, String message) {
        if (minimum > maximum) {
            throw new IllegalArgumentException(message);
        }
    }

    private DeviceConfigResponse toConfigResponse(DeviceConfig config) {
        return toConfigResponse(config, null);
    }

    private DeviceConfigResponse toConfigResponse(DeviceConfig config, String commandId) {
        return new DeviceConfigResponse(
                config.getDeviceId(),
                config.getAutoMode() != 0,
                config.getSoilMoistureMin(),
                config.getSoilMoistureMax(),
                config.getMaxWateringDuration(),
                config.getAirHumidityMin(),
                config.getAirHumidityMax(),
                config.getAirTemperatureMin(),
                config.getAirTemperatureMax(),
                config.getWaterLevelMin(),
                commandId,
                commandId == null ? null : "PENDING");
    }

    private ScheduleResponse toScheduleResponse(Schedule schedule) {
        return new ScheduleResponse(
                schedule.getId(),
                schedule.getDevice().getId(),
                schedule.getStartTime(),
                schedule.getEndTime(),
                schedule.getDurationMinutes(),
                schedule.getDaysOfWeek(),
                schedule.isEnabled());
    }

    private Map<String, Object> configPayload(DeviceConfig config) {
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("auto_mode", config.getAutoMode() != 0);
        payload.put("soil_moisture_min", config.getSoilMoistureMin());
        payload.put("soil_moisture_max", config.getSoilMoistureMax());
        payload.put("max_watering_duration_minutes", config.getMaxWateringDuration());
        payload.put("air_humidity_min", config.getAirHumidityMin());
        payload.put("air_humidity_max", config.getAirHumidityMax());
        payload.put("air_temperature_min", config.getAirTemperatureMin());
        payload.put("air_temperature_max", config.getAirTemperatureMax());
        payload.put("min_water_level_alert", config.getWaterLevelMin());
        return payload;
    }
}
