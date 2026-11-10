
import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/repositories/i_automation_repository.dart';
import '../models/device_config_model.dart';
import '../models/schedule_model.dart';

class AutomationRepositoryImpl implements IAutomationRepository {
  AutomationRepositoryImpl(this._apiClient);

  final ApiClient _apiClient;

  String _devicePath(String deviceId) =>
      '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}';

  @override
  Future<DeviceConfigModel> getDeviceConfig(String deviceId) async {
    try {
      final response = await _apiClient.get('${_devicePath(deviceId)}/config');
      return DeviceConfigModel.fromJson(response);
    } on ApiException catch (e) {
      // Backend trả 404 khi thiết bị chưa có cấu hình; nó chỉ tạo cấu hình khi lưu lần đầu.
      // Nếu thiết bị không tồn tại thì getSchedules vẫn báo lỗi 404.
      if (e.statusCode != 404) rethrow;
      return DeviceConfigModel.fromJson({'device_id': deviceId});
    }
  }

  @override
  Future<bool> updateDeviceConfig(DeviceConfigModel config) async {
    await _apiClient.put(
      '${_devicePath(config.deviceId)}/config',
      body: config.toJson(),
    );
    return true;
  }

  @override
  Future<bool> setWateringMode(String deviceId, {required bool automatic}) async {
    await _apiClient.post(
      '${_devicePath(deviceId)}/mode',
      body: {'mode': automatic ? 'auto' : 'manual'},
    );
    return true;
  }

  @override
  Future<List<ScheduleModel>> getSchedules(String deviceId) async {
    final response = await _apiClient.get('${_devicePath(deviceId)}/schedules');
    final rows = response['data'];
    if (rows is! List) return const [];
    return rows
        .whereType<Map<String, dynamic>>()
        .map(ScheduleModel.fromJson)
        .toList();
  }

  @override
  Future<bool> addSchedule(ScheduleModel schedule) async {
    await _apiClient.post(
      '${_devicePath(schedule.deviceId)}/schedules',
      body: _scheduleRequest(schedule),
    );
    return true;
  }

  @override
  Future<bool> updateSchedule(ScheduleModel schedule) async {
    if (schedule.id == null) {
      throw ArgumentError('Lịch tưới cần có mã để cập nhật.');
    }
    await _apiClient.put(
      '${_devicePath(schedule.deviceId)}/schedules/${schedule.id}',
      body: _scheduleRequest(schedule),
    );
    return true;
  }

  @override
  Future<bool> deleteSchedule(String deviceId, int scheduleId) async {
    await _apiClient.delete(
      '${_devicePath(deviceId)}/schedules/$scheduleId',
    );
    return true;
  }

  @override
  Future<void> processOfflineQueue() async {
    // The backend and device handle offline command delivery.
  }

  Map<String, dynamic> _scheduleRequest(ScheduleModel schedule) => {
        'start_time': schedule.startTime,
        'end_time': schedule.endTime,
        'duration_minutes': schedule.durationMinutes,
        'days_of_week': schedule.daysOfWeek,
        'is_enabled': schedule.isEnabled,
      };
}
