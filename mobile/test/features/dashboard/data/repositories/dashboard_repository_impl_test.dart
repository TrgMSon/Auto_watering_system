import 'dart:async';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/dashboard/data/models/telemetry_model.dart';
import 'package:watering_app/features/dashboard/data/repositories/dashboard_repository_impl.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  late MockApiClient mockApiClient;

  setUp(() {
    mockApiClient = MockApiClient();
  });

  group('DashboardRepositoryImpl.watchTelemetry', () {
    test('emits error on first poll failure and then emits telemetry on next poll success', () async {
      var callCount = 0;
      when(() => mockApiClient.get(any())).thenAnswer((_) async {
        callCount++;
        if (callCount == 1) {
          throw const ApiException(500, 'Server error');
        }
        return {
          'device_id': 'ESP_001',
          'soil_moisture': 45.0,
          'temperature': 25.0,
          'humidity': 60.0,
          'water_level': 80.0,
          'recorded_at': DateTime(2026, 11, 2, 10, 0).toIso8601String(),
        };
      });

      final repository = DashboardRepositoryImpl(
        mockApiClient,
        pollInterval: const Duration(milliseconds: 10),
      );

      final stream = repository.watchTelemetry('ESP_001', null);

      final errors = <Object>[];
      final data = <TelemetryModel?>[];
      final completer = Completer<void>();

      final subscription = stream.listen(
        (telemetry) {
          data.add(telemetry);
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
      expect(data.first?.deviceId, equals('ESP_001'));
      expect(data.first?.soilMoisture, equals(45.0));
    });

    test('emits null when API returns empty payload (meaning no telemetry yet)', () async {
      when(() => mockApiClient.get(any())).thenAnswer((_) async => {});

      final repository = DashboardRepositoryImpl(
        mockApiClient,
        pollInterval: const Duration(milliseconds: 10),
      );

      final stream = repository.watchTelemetry('ESP_001', null);
      final completer = Completer<void>();
      final data = <TelemetryModel?>[];

      final subscription = stream.listen((telemetry) {
        data.add(telemetry);
        if (!completer.isCompleted) completer.complete();
      });

      await completer.future.timeout(const Duration(seconds: 2));
      await subscription.cancel();

      expect(data, hasLength(1));
      expect(data.first, isNull);
    });

    test('isPumpOn becomes true when pump is turned on, and false after duration expires', () async {
      when(() => mockApiClient.post(any(), body: any(named: 'body'))).thenAnswer((_) async => {
        'command_id': 'cmd-1',
      });
      when(() => mockApiClient.get(any())).thenAnswer((invocation) async {
        final path = invocation.positionalArguments[0] as String;
        if (path.contains('/ack')) {
          return {'status': 'SUCCEEDED'};
        }
        return {
          'device_id': 'ESP_001',
          'soil_moisture': 45.0,
          'temperature': 25.0,
          'humidity': 60.0,
          'water_level': 80.0,
          'recorded_at': DateTime.now().toIso8601String(),
        };
      });

      final repository = DashboardRepositoryImpl(
        mockApiClient,
        pollInterval: const Duration(milliseconds: 20),
      );

      // Duration: 1 second
      await repository.togglePump('ESP_001', true, duration: 1);

      final stream = repository.watchTelemetry('ESP_001', null);
      final readings = <TelemetryModel?>[];
      final completer = Completer<void>();

      final subscription = stream.listen((telemetry) {
        readings.add(telemetry);
        if (readings.any((r) => r?.isPumpOn == true) &&
            readings.any((r) => r?.isPumpOn == false) &&
            !completer.isCompleted) {
          completer.complete();
        }
      });

      await completer.future.timeout(const Duration(seconds: 3));
      await subscription.cancel();

      expect(readings.any((r) => r?.isPumpOn == true), isTrue);
      expect(readings.last?.isPumpOn, isFalse);
    });
  });
}

