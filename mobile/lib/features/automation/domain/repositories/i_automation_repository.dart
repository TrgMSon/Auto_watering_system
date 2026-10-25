import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';

abstract class IAutomationRepository {
  /// Fetch the backend configuration for a specific device.
  Future<DeviceConfigModel> getDeviceConfig(String deviceId);
  
  /// Update the configuration through the backend and MQTT command publisher.
  Future<bool> updateDeviceConfig(DeviceConfigModel config);

  /// Fetch all schedules for a device
  Future<List<ScheduleModel>> getSchedules(String deviceId);

  /// Add a schedule for the device.
  Future<bool> addSchedule(ScheduleModel schedule);

  /// Update an existing device schedule.
  Future<bool> updateSchedule(ScheduleModel schedule);

  /// Delete a schedule belonging to the device.
  Future<bool> deleteSchedule(String deviceId, int scheduleId);

  /// Kept for compatibility; the backend/device handle offline command delivery.
  Future<void> processOfflineQueue();
}
