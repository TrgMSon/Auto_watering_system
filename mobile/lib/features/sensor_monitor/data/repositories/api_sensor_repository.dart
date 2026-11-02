import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../../dashboard/data/models/device_model.dart';
import '../../domain/entities/sensor_reading.dart';
import '../../domain/repositories/sensor_repository.dart';

class ApiSensorRepository implements SensorRepository {
  ApiSensorRepository(
    this._apiClient, {
    Duration pollInterval = const Duration(seconds: 5),
  }) : _pollInterval = pollInterval;

  final ApiClient _apiClient;
  final Duration _pollInterval;

  Future<String?> _getDeviceId() async {
    final response = await _apiClient.get(ApiEndpoints.devices);
    final rows = response['data'];
    if (rows is! List || rows.isEmpty) return null;
    final first = rows.first;
    if (first is! Map<String, dynamic>) return null;
    return DeviceModel.fromJson(first).id;
  }

  @override
  Future<List<SensorReading>> getSensorHistory({
    required DateTime from,
    required DateTime to,
  }) async {
    final deviceId = await _getDeviceId();
    if (deviceId == null) return const [];
    final response = await _apiClient.get(
      '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/telemetry',
      queryParameters: {
        'from': from.toLocal().toIso8601String(),
        'to': to.toLocal().toIso8601String(),
      },
    );
    final rows = response['data'];
    if (rows is! List) return const [];
    return rows
        .whereType<Map<String, dynamic>>()
        .map(_readingFromJson)
        .toList();
  }

  @override
  Stream<SensorReading> getRealtimeReadings() async* {
    String? cachedDeviceId;
    while (true) {
      try {
        cachedDeviceId ??= await _getDeviceId();
        if (cachedDeviceId != null) {
          final response = await _apiClient.get(
            '${ApiEndpoints.devices}/${Uri.encodeComponent(cachedDeviceId)}/telemetry/latest',
          );
          if (response.isNotEmpty) yield _readingFromJson(response);
        }
      } catch (e, stackTrace) {
        yield* Stream<SensorReading>.error(e, stackTrace);
      }
      await Future<void>.delayed(_pollInterval);
    }
  }

  SensorReading _readingFromJson(Map<String, dynamic> json) {
    final timestamp = json['recorded_at']?.toString();
    return SensorReading(
      id: json['id'] is num ? (json['id'] as num).toInt() : 0,
      soilMoisture: _number(json['soil_moisture']),
      temperature: _number(json['temperature']),
      humidity: _number(json['humidity']),
      lightIntensity: 0,
      timestamp: timestamp == null ? DateTime.now() : DateTime.parse(timestamp),
    );
  }

  double _number(dynamic value) =>
      value is num ? value.toDouble() : double.tryParse('$value') ?? 0;
}
