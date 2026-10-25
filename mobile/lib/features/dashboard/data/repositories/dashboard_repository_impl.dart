import 'dart:async';

import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../models/device_model.dart';
import '../models/telemetry_model.dart';
import '../../domain/repositories/dashboard_repository.dart';
import '../../domain/entities/dashboard_summary.dart';

class DashboardRepositoryImpl implements DashboardRepository {
  DashboardRepositoryImpl(this._apiClient);

  final ApiClient _apiClient;
  final Map<String, bool> _pumpStatus = {};

  @override
  Future<List<DeviceModel>> getDevices() async {
    final response = await _apiClient.get(ApiEndpoints.devices);
    final rows = response['data'];
    if (rows is! List) return const [];
    return rows
        .whereType<Map<String, dynamic>>()
        .map(DeviceModel.fromJson)
        .toList();
  }

  @override
  Future<void> togglePump(String deviceId, bool turnOn, {int duration = 120}) async {
    final response = await _apiClient.post(
      '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/control',
      body: {
        'action': turnOn ? 'ON' : 'OFF',
        if (turnOn) 'duration_seconds': duration,
      },
    );
    final commandId = response['command_id']?.toString();
    if (commandId == null || commandId.isEmpty) {
      throw const ApiException(500, 'Backend không trả về mã xác nhận lệnh bơm.');
    }

    for (var attempt = 0; attempt < 10; attempt++) {
      await Future<void>.delayed(const Duration(milliseconds: 500));
      final acknowledgment = await _apiClient.get(
        '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/commands/${Uri.encodeComponent(commandId)}/ack',
      );
      final status = acknowledgment['status']?.toString().toUpperCase();
      if (status == 'SUCCEEDED') {
        _pumpStatus[deviceId] = turnOn;
        return;
      }
      if (status == 'FAILED' || status == 'PUBLISH_FAILED') {
        throw ApiException(
          502,
          acknowledgment['message']?.toString() ?? 'Thiết bị từ chối lệnh bơm.',
        );
      }
    }
    throw const ApiException(
      408,
      'Chưa nhận được xác nhận từ thiết bị. Vui lòng kiểm tra trạng thái thiết bị.',
    );
  }

  @override
  Stream<TelemetryModel> watchTelemetry(String deviceId, String? blePassKey) async* {
    while (true) {
      final response = await _apiClient.get(
        '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/telemetry/latest',
      );
      if (response.isNotEmpty) {
        yield TelemetryModel.fromJson(response).copyWith(
          isPumpOn: _pumpStatus[deviceId],
        );
      }
      await Future<void>.delayed(const Duration(seconds: 5));
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
