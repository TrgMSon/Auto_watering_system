import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';

abstract class IAutomationRepository {
  /// Fetch the config for a specific device. 
  /// Usually parsed from the /api/v1/devices payload or a specific endpoint.
  Future<DeviceConfigModel> getDeviceConfig(String deviceId);
  
  /// Update the config using Dual Sync (BLE + HTTP Queue).
  /// Optimistically returns true if BLE or Local Queue accepts it.
  Future<bool> updateDeviceConfig(DeviceConfigModel config);

  /// Fetch all schedules for a device
  Future<List<ScheduleModel>> getSchedules(String deviceId);

  /// Add a new schedule (POST /api/v1/schedules) with Dual Sync
  Future<bool> addSchedule(ScheduleModel schedule);

  /// Update an existing schedule (PUT /api/v1/schedules/{id})
  Future<bool> updateSchedule(ScheduleModel schedule);

  /// Delete a schedule (DELETE /api/v1/schedules/{id})
  Future<bool> deleteSchedule(int scheduleId);

  /// Run the queue processor when network becomes available
  Future<void> processOfflineQueue();
}
