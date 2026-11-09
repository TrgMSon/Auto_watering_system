import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/sensor_monitor/data/repositories/api_sensor_repository.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/sensor_reading.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  late MockApiClient mockApiClient;

  setUp(() {
    mockApiClient = MockApiClient();
  });

  group('ApiSensorRepository.getRealtimeReadings', () {
    test(
        'emits error on first poll failure and then emits sensor reading on next poll success',
        () async {
      var callCount = 0;
      when(() => mockApiClient.get(any())).thenAnswer((invocation) async {
        final path = invocation.positionalArguments.first as String;
        if (path.contains('/telemetry/latest')) {
          callCount++;
          if (callCount == 1) {
            throw const ApiException(500, 'Server error');
          }
          return {
            'id': 1,
            'soil_moisture': 45.0,
            'temperature': 25.0,
            'humidity': 60.0,
            'recorded_at': DateTime(2026, 11, 2, 10, 0).toIso8601String(),
          };
        }
        return {};
      });

      final repository = ApiSensorRepository(
        mockApiClient,
        pollInterval: const Duration(milliseconds: 10),
      );

      final stream = repository.getRealtimeReadings('ESP_001');

      final errors = <Object>[];
      final data = <SensorReading>[];
      final completer = Completer<void>();

      final subscription = stream.listen(
        (reading) {
          data.add(reading);
          if (data.isNotEmpty && errors.isNotEmpty && !completer.isCompleted) {
            completer.complete();
          }
        },
        onError: (e) {
          errors.add(e);
        },
      );

      await completer.future.timeout(const Duration(seconds: 2));
      await subscription.cancel();

      expect(errors, hasLength(1));
      expect(errors.first, isA<ApiException>());
      expect((errors.first as ApiException).message, equals('Server error'));
      expect(data, hasLength(1));
      expect(data.first.id, equals(1));
      expect(data.first.soilMoisture, equals(45.0));
      expect(data.first.temperature, equals(25.0));
    });

    test('returns empty stream when deviceId is empty', () async {
      final repository = ApiSensorRepository(
        mockApiClient,
        pollInterval: const Duration(milliseconds: 10),
      );

      final stream = repository.getRealtimeReadings('');
      final readings = await stream.toList();

      expect(readings, isEmpty);
      verifyNever(() => mockApiClient.get(any()));
    });
  });

  group('ApiSensorRepository.getSensorHistory', () {
    test('fetches telemetry with encoded deviceId and date query parameters',
        () async {
      final from = DateTime(2026, 11, 1, 0, 0);
      final to = DateTime(2026, 11, 2, 0, 0);

      when(() => mockApiClient.get(
            any(),
            queryParameters: any(named: 'queryParameters'),
          )).thenAnswer((_) async => {
            'data': [
              {
                'id': 101,
                'soil_moisture': 55.0,
                'temperature': 27.5,
                'humidity': 62.0,
                'recorded_at': DateTime(2026, 11, 1, 12, 0).toIso8601String(),
              }
            ]
          });

      final repository = ApiSensorRepository(mockApiClient);
      final result = await repository.getSensorHistory(
        deviceId: 'ESP_001',
        from: from,
        to: to,
      );

      expect(result, hasLength(1));
      expect(result.first.id, equals(101));
      expect(result.first.soilMoisture, equals(55.0));
      verify(() => mockApiClient.get(
            '/devices/ESP_001/telemetry',
            queryParameters: {
              'from': from.toLocal().toIso8601String(),
              'to': to.toLocal().toIso8601String(),
            },
          )).called(1);
    });

    test('returns empty list when deviceId is empty without calling API',
        () async {
      final repository = ApiSensorRepository(mockApiClient);
      final result = await repository.getSensorHistory(
        deviceId: '',
        from: DateTime.now(),
        to: DateTime.now(),
      );

      expect(result, isEmpty);
      verifyNever(() => mockApiClient.get(
            any(),
            queryParameters: any(named: 'queryParameters'),
          ));
    });
  });
}
