
import '../../domain/repositories/i_automation_repository.dart';
import '../models/device_config_model.dart';
import '../models/schedule_model.dart';

class AutomationRepositoryImpl implements IAutomationRepository {
  // Mock in-memory data for UI testing
  DeviceConfigModel _mockConfig = const DeviceConfigModel(
    deviceId: 'ESP_001',
    autoMode: true,
    soilMoistureMin: 40.0,
    soilMoistureMax: 80.0,
    airHumidityMin: 50.0,
    airHumidityMax: 80.0,
    temperatureMin: 22.0,
    temperatureMax: 35.0,
    maxWateringDuration: 120,
  );

  final List<ScheduleModel> _mockSchedules = [
    const ScheduleModel(
      id: 1,
      deviceId: 'ESP_001',
      startTime: '06:00:00',
      endTime: '06:15:00',
      daysOfWeek: 'MON,WED,FRI',
      isEnabled: true,
    ),
  ];

  @override
  Future<DeviceConfigModel> getDeviceConfig(String deviceId) async {
    await Future.delayed(const Duration(milliseconds: 500));
    return _mockConfig;
  }

  @override
  Future<bool> updateDeviceConfig(DeviceConfigModel config) async {
    _mockConfig = config; 
    
    try {
      await Future.delayed(const Duration(milliseconds: 300));
      return true;
    } catch (e) {
      throw Exception("Không thể kết nối đến thiết bị hoặc máy chủ.");
    }
  }

  @override
  Future<List<ScheduleModel>> getSchedules(String deviceId) async {
    await Future.delayed(const Duration(milliseconds: 500));
    return List.from(_mockSchedules);
  }

  @override
  Future<bool> addSchedule(ScheduleModel schedule) async {
    final newSchedule = ScheduleModel(
      id: DateTime.now().millisecondsSinceEpoch,
      deviceId: schedule.deviceId,
      startTime: schedule.startTime,
      endTime: schedule.endTime,
      daysOfWeek: schedule.daysOfWeek,
      isEnabled: schedule.isEnabled,
    );
    _mockSchedules.add(newSchedule);

    try {
      await Future.delayed(const Duration(milliseconds: 300));
      return true;
    } catch (e) {
      throw Exception("Không thể lưu lịch hẹn giờ.");
    }
  }

  @override
  Future<bool> updateSchedule(ScheduleModel schedule) async {
    final index = _mockSchedules.indexWhere((s) => s.id == schedule.id);
    if (index != -1) {
      _mockSchedules[index] = schedule;
    }
    
    try {
      await Future.delayed(const Duration(milliseconds: 300));
      return true;
    } catch (e) {
      throw Exception("Không thể cập nhật lịch hẹn giờ.");
    }
  }

  @override
  Future<bool> deleteSchedule(int scheduleId) async {
    _mockSchedules.removeWhere((s) => s.id == scheduleId);
    
    try {
      // Giả lập API DELETE
      await Future.delayed(const Duration(milliseconds: 300));
      return true;
    } catch (e) {
      throw Exception("Không thể xóa lịch hẹn giờ.");
    }
  }

  @override
  Future<void> processOfflineQueue() async {
    // Hàm này không còn cần thiết vì ESP32 sẽ đảm nhận việc lưu trữ khi mất Wi-Fi.
    // Giữ lại interface method rỗng hoặc có thể xóa luôn ở IAutomationRepository sau.
  }
}
