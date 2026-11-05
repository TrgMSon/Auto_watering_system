import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/sensor_monitor/data/repositories/api_sensor_repository.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/statistics_point.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  const query = {'deviceId': 'AA:BB', 'range': '7d'};

  late MockApiClient apiClient;
  late ApiSensorRepository repository;

  setUp(() {
    apiClient = MockApiClient();
    repository = ApiSensorRepository(apiClient);
  });

  void stub(String path, List<Map<String, dynamic>> rows) {
    when(() => apiClient.get(path, queryParameters: query))
        .thenAnswer((_) async => {'data': rows});
  }

  test('merges both APIs by hour, in time order, and keeps missing values null', () async {
    stub('/statistics/environmental', [
      {
        'timeLabel': '2026-11-05T11:00:00',
        'avgSoilMoisture': 41.5,
        'avgTemperature': null,
        'avgHumidity': 60,
      },
      {
        'timeLabel': '2026-11-05T10:00:00',
        'avgSoilMoisture': 40,
        'avgTemperature': 30,
        'avgHumidity': 61,
      },
    ]);
    stub('/statistics/water-level', [
      {'timeLabel': '2026-11-05T10:00:00', 'avgWaterLevel': 12.5},
      {'timeLabel': '2026-11-05T12:00:00', 'avgWaterLevel': 11},
    ]);

    final points = await repository.getStatistics(
      deviceId: 'AA:BB',
      range: StatisticsRange.week,
    );

    expect(points, [
      StatisticsPoint(
        time: DateTime(2026, 11, 5, 10),
        soilMoisture: 40,
        temperature: 30,
        humidity: 61,
        waterLevel: 12.5,
      ),
      StatisticsPoint(time: DateTime(2026, 11, 5, 11), soilMoisture: 41.5, humidity: 60),
      StatisticsPoint(time: DateTime(2026, 11, 5, 12), waterLevel: 11),
    ]);
  });

  test('skips rows without a valid time', () async {
    stub('/statistics/environmental', [
      {'timeLabel': null, 'avgSoilMoisture': 40},
    ]);
    stub('/statistics/water-level', []);

    final points = await repository.getStatistics(
      deviceId: 'AA:BB',
      range: StatisticsRange.week,
    );

    expect(points, isEmpty);
  });

  test('does not call the API without a device', () async {
    final points = await repository.getStatistics(deviceId: '', range: StatisticsRange.day);

    expect(points, isEmpty);
    verifyZeroInteractions(apiClient);
  });
}
