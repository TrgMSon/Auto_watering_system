import '../entities/dashboard_summary.dart';
import '../../data/models/device_model.dart';
import '../../data/models/telemetry_model.dart';

abstract class DashboardRepository {
  Future<DashboardSummary> getDashboardSummary();
  Future<bool> toggleAutoWatering({required bool enabled});

  // New API-first methods for Tab 1
  Future<List<DeviceModel>> getDevices();
  Future<void> togglePump(String deviceId, bool turnOn, {int duration = 120});
  Stream<TelemetryModel> watchTelemetry(String deviceId, String? blePassKey);
}
