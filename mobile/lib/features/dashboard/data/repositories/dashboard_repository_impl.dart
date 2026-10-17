import 'dart:async';
import 'dart:math';
import '../models/device_model.dart';
import '../models/telemetry_model.dart';
import '../../domain/repositories/dashboard_repository.dart';
import '../../domain/entities/dashboard_summary.dart';

class DashboardRepositoryImpl implements DashboardRepository {
  // Temporary mock implementation for UI testing
  bool _isPumpOn = false;

  @override
  Future<List<DeviceModel>> getDevices() async {
    await Future.delayed(const Duration(milliseconds: 500));
    return const [
      DeviceModel(
        id: 'ESP_001',
        deviceName: 'Vườn Lan Ban Công',
        status: 'ONLINE',
        permission: 'OWNER',
        blePassKey: '123456',
      ),
      DeviceModel(
        id: 'ESP_002',
        deviceName: 'Vườn Hồng Sân Thượng',
        status: 'OFFLINE',
        permission: 'VIEWER',
      ),
    ];
  }

  @override
  Future<void> togglePump(String deviceId, bool turnOn, {int duration = 120}) async {
    // API Call: POST /api/v1/devices/{deviceId}/control
    // Payload: {"command": turnOn ? "PUMP_ON" : "PUMP_OFF", "duration_seconds": duration}
    await Future.delayed(const Duration(milliseconds: 600));
    _isPumpOn = turnOn;
  }

  @override
  Stream<TelemetryModel> watchTelemetry(String deviceId, String? blePassKey) async* {
    // Hybrid logic: Attempt BLE first, if fails fallback to HTTP Polling
    // Currently simulated using Stream.periodic
    
    final random = Random();
    
    while (true) {
      await Future.delayed(const Duration(seconds: 5));
      
      yield TelemetryModel(
        deviceId: deviceId,
        soilMoisture: 40.0 + random.nextDouble() * 20, // 40-60%
        temperature: 25.0 + random.nextDouble() * 10,  // 25-35C
        humidity: 60.0 + random.nextDouble() * 30,     // 60-90%
        waterLevel: 80.0 - random.nextDouble() * 5,    // 75-80%
        isPumpOn: _isPumpOn,
        recordedAt: DateTime.now(),
      );
    }
  }

  // Legacy methods
  @override
  Future<DashboardSummary> getDashboardSummary() async {
    throw UnimplementedError('Use watchTelemetry instead');
  }

  @override
  Future<bool> toggleAutoWatering({required bool enabled}) async {
    throw UnimplementedError('Moved to Automation Tab');
  }
}
