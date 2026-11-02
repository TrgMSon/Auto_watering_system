import 'dart:async';
import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/sensor_reading.dart';
import 'package:watering_app/features/sensor_monitor/domain/usecases/get_realtime_readings_usecase.dart';
import 'package:watering_app/features/sensor_monitor/domain/usecases/get_sensor_history_usecase.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_event.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_state.dart';

class MockGetSensorHistoryUseCase extends Mock
    implements GetSensorHistoryUseCase {}

class MockGetRealtimeReadingsUseCase extends Mock
    implements GetRealtimeReadingsUseCase {}

void main() {
  late MockGetSensorHistoryUseCase mockGetSensorHistory;
  late MockGetRealtimeReadingsUseCase mockGetRealtimeReadings;

  final testReading1 = SensorReading(
    id: 1,
    soilMoisture: 45.0,
    temperature: 25.0,
    humidity: 60.0,
    lightIntensity: 0,
    timestamp: DateTime(2026, 11, 2, 10, 0),
  );

  final testReading2 = SensorReading(
    id: 2,
    soilMoisture: 46.0,
    temperature: 26.0,
    humidity: 61.0,
    lightIntensity: 0,
    timestamp: DateTime(2026, 11, 2, 10, 5),
  );

  setUp(() {
    mockGetSensorHistory = MockGetSensorHistoryUseCase();
    mockGetRealtimeReadings = MockGetRealtimeReadingsUseCase();
  });

  group('SensorBloc - SensorHistoryRequested', () {
    final from = DateTime(2026, 11, 1);
    final to = DateTime(2026, 11, 2);

    blocTest<SensorBloc, SensorState>(
      'calls getSensorHistory with proper deviceId and emits [SensorLoading, SensorLoaded]',
      build: () {
        when(() => mockGetSensorHistory(
              deviceId: 'ESP_001',
              from: from,
              to: to,
            )).thenAnswer((_) async => [testReading1]);
        return SensorBloc(
          getSensorHistory: mockGetSensorHistory,
          getRealtimeReadings: mockGetRealtimeReadings,
        );
      },
      act: (bloc) => bloc.add(SensorHistoryRequested(
        deviceId: 'ESP_001',
        from: from,
        to: to,
      )),
      expect: () => [
        const SensorLoading(),
        SensorLoaded(
          historyReadings: [testReading1],
          latestReading: testReading1,
        ),
      ],
      verify: (_) {
        verify(() => mockGetSensorHistory(
              deviceId: 'ESP_001',
              from: from,
              to: to,
            )).called(1);
      },
    );

    blocTest<SensorBloc, SensorState>(
      'emits [SensorLoading, SensorError] when history fetch throws an exception',
      build: () {
        when(() => mockGetSensorHistory(
              deviceId: 'ESP_001',
              from: from,
              to: to,
            )).thenThrow(Exception('Network error'));
        return SensorBloc(
          getSensorHistory: mockGetSensorHistory,
          getRealtimeReadings: mockGetRealtimeReadings,
        );
      },
      act: (bloc) => bloc.add(SensorHistoryRequested(
        deviceId: 'ESP_001',
        from: from,
        to: to,
      )),
      expect: () => [
        const SensorLoading(),
        const SensorError(message: 'Exception: Network error'),
      ],
    );
  });

  group('SensorBloc - SensorRealtimeStarted', () {
    test('calls getRealtimeReadings with deviceId and cancels previous stream on device change', () async {
      final controller1 = StreamController<SensorReading>();
      final controller2 = StreamController<SensorReading>();

      when(() => mockGetRealtimeReadings('ESP_001'))
          .thenAnswer((_) => controller1.stream);
      when(() => mockGetRealtimeReadings('ESP_002'))
          .thenAnswer((_) => controller2.stream);

      final bloc = SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      );

      bloc.add(const SensorRealtimeStarted(deviceId: 'ESP_001'));
      await pumpEventQueue();

      expect(controller1.hasListener, isTrue);
      verify(() => mockGetRealtimeReadings('ESP_001')).called(1);

      // Change device to ESP_002
      bloc.add(const SensorRealtimeStarted(deviceId: 'ESP_002'));
      await pumpEventQueue();

      expect(controller1.hasListener, isFalse);
      expect(controller2.hasListener, isTrue);
      verify(() => mockGetRealtimeReadings('ESP_002')).called(1);

      // Stop realtime
      bloc.add(const SensorRealtimeStopped());
      await pumpEventQueue();

      expect(controller2.hasListener, isFalse);

      await bloc.close();
      await controller1.close();
      await controller2.close();
    });

    blocTest<SensorBloc, SensorState>(
      'appends reading to realtimeBuffer and updates latestReading',
      build: () {
        when(() => mockGetRealtimeReadings('ESP_001'))
            .thenAnswer((_) => Stream.fromIterable([testReading1, testReading2]));
        return SensorBloc(
          getSensorHistory: mockGetSensorHistory,
          getRealtimeReadings: mockGetRealtimeReadings,
        );
      },
      act: (bloc) => bloc.add(const SensorRealtimeStarted(deviceId: 'ESP_001')),
      expect: () => [
        SensorLoaded(
          latestReading: testReading1,
          realtimeBuffer: [testReading1],
        ),
        SensorLoaded(
          latestReading: testReading2,
          realtimeBuffer: [testReading1, testReading2],
        ),
      ],
    );
  });
}
