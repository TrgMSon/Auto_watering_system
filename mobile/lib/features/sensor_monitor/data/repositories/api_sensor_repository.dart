import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/entities/sensor_reading.dart';
import '../../domain/entities/statistics_point.dart';
import '../../domain/repositories/sensor_repository.dart';

class ApiSensorRepository implements SensorRepository {
  ApiSensorRepository(
    this._apiClient, {
    Duration pollInterval = const Duration(seconds: 5),
  }) : _pollInterval = pollInterval;

  final ApiClient _apiClient;
  final Duration _pollInterval;

  @override
  Future<List<SensorReading>> getSensorHistory({
    required String deviceId,
    required DateTime from,
    required DateTime to,
  }) async {
    if (deviceId.isEmpty) return const [];
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
  Stream<SensorReading> getRealtimeReadings(String deviceId) async* {
    if (deviceId.isEmpty) return;
    while (true) {
      try {
        final response = await _apiClient.get(
          '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/telemetry/latest',
        );
        if (response.isNotEmpty) yield _readingFromJson(response);
      } catch (e, stackTrace) {
        yield* Stream<SensorReading>.error(e, stackTrace);
      }
      await Future<void>.delayed(_pollInterval);
    }
  }

  @override
  Future<List<StatisticsPoint>> getStatistics({
    required String deviceId,
    required StatisticsRange range,
  }) async {
    if (deviceId.isEmpty) return const [];
    final queryParameters = {'deviceId': deviceId, 'range': range.apiValue};
    final responses = await Future.wait([
      _apiClient.get(
        '${ApiEndpoints.statistics}/environmental',
        queryParameters: queryParameters,
      ),
      _apiClient.get(
        '${ApiEndpoints.statistics}/water-level',
        queryParameters: queryParameters,
      ),
    ]);

    final points = <DateTime, StatisticsPoint>{};
    for (final json in _rows(responses[0])) {
      final time = DateTime.tryParse(json['timeLabel']?.toString() ?? '');
      if (time == null) continue;
      points[time] = StatisticsPoint(
        time: time,
        soilMoisture: _optionalNumber(json['avgSoilMoisture']),
        temperature: _optionalNumber(json['avgTemperature']),
        humidity: _optionalNumber(json['avgHumidity']),
      );
    }
    for (final json in _rows(responses[1])) {
      final time = DateTime.tryParse(json['timeLabel']?.toString() ?? '');
      if (time == null) continue;
      final point = points[time];
      points[time] = StatisticsPoint(
        time: time,
        soilMoisture: point?.soilMoisture,
        temperature: point?.temperature,
        humidity: point?.humidity,
        waterLevel: _optionalNumber(json['avgWaterLevel']),
      );
    }
    return points.values.toList()..sort((a, b) => a.time.compareTo(b.time));
  }

  Iterable<Map<String, dynamic>> _rows(Map<String, dynamic> response) {
    final rows = response['data'];
    return rows is List ? rows.whereType<Map<String, dynamic>>() : const [];
  }

  double? _optionalNumber(dynamic value) => value is num ? value.toDouble() : null;

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
