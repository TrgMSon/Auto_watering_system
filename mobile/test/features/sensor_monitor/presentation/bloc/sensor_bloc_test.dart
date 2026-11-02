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

  final testReading3 = SensorReading(
    id: 3,
    soilMoisture: 47.0,
    temperature: 27.0,
    humidity: 62.0,
    lightIntensity: 0,
    timestamp: DateTime(2026, 11, 2, 10, 10),
  );

  setUp(() {
    mockGetSensorHistory = MockGetSensorHistoryUseCase();
    mockGetRealtimeReadings = MockGetRealtimeReadingsUseCase();
  });

  group('SensorLoaded.chartReadings', () {
    test('merges history and realtime readings without duplicates and sorted by timestamp', () {
      final state = SensorLoaded(
        historyReadings: [testReading1, testReading2],
        latestReading: testReading3,
        realtimeBuffer: [testReading2, testReading3], // testReading2 duplicated
      );

      final chart = state.chartReadings;
      expect(chart, hasLength(3));
      expect(chart.map((r) => r.id).toList(), equals([1, 2, 3]));
      expect(chart[0].timestamp.isBefore(chart[1].timestamp), isTrue);
      expect(chart[1].timestamp.isBefore(chart[2].timestamp), isTrue);
    });
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

    test(
      'ignores results of stale request when device is changed before previous request completes',
      () async {
        final completerA = Completer<List<SensorReading>>();

        when(() => mockGetSensorHistory(
              deviceId: 'ESP_001',
              from: any(named: 'from'),
              to: any(named: 'to'),
            )).thenAnswer((_) => completerA.future);

        when(() => mockGetSensorHistory(
              deviceId: 'ESP_002',
              from: any(named: 'from'),
              to: any(named: 'to'),
            )).thenAnswer((_) async => [testReading2]);

        final bloc = SensorBloc(
          getSensorHistory: mockGetSensorHistory,
          getRealtimeReadings: mockGetRealtimeReadings,
        );

        // Request device A (pending)
        bloc.add(SensorHistoryRequested(
          deviceId: 'ESP_001',
          from: from,
          to: to,
        ));
        await pumpEventQueue();

        // Switch to device B (finishes first)
        bloc.add(SensorHistoryRequested(
          deviceId: 'ESP_002',
          from: from,
          to: to,
        ));
        await pumpEventQueue();

        // Device B has finished and state is loaded with B's data
        expect(bloc.state, isA<SensorLoaded>());
        expect((bloc.state as SensorLoaded).historyReadings, equals([testReading2]));

        // Device A finishes later
        completerA.complete([testReading1]);
        await pumpEventQueue();

        // State must STILL be device B's data, NOT overwritten by device A
        expect(bloc.state, isA<SensorLoaded>());
        expect((bloc.state as SensorLoaded).historyReadings, equals([testReading2]));

        await bloc.close();
      },
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

    blocTest<SensorBloc, SensorState>(
      'ignores realtime reading when id is identical to latestReading',
      seed: () => SensorLoaded(
        historyReadings: [testReading1],
        latestReading: testReading1,
        realtimeBuffer: [testReading1],
      ),
      build: () => SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      ),
      act: (bloc) => bloc.add(SensorRealtimeDataReceived(reading: testReading1)),
      expect: () => [], // No new state emitted, buffer does not grow
    );
  });

  group('SensorBloc - SensorRealtimeFailed', () {
    blocTest<SensorBloc, SensorState>(
      'preserves loaded data and sets errorMessage on SensorLoaded',
      seed: () => SensorLoaded(
        historyReadings: [testReading1],
        latestReading: testReading1,
        realtimeBuffer: [testReading1],
      ),
      build: () => SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      ),
      act: (bloc) => bloc.add(const SensorRealtimeFailed(message: 'Timeout')),
      expect: () => [
        SensorLoaded(
          historyReadings: [testReading1],
          latestReading: testReading1,
          realtimeBuffer: [testReading1],
          errorMessage: 'Timeout',
        ),
      ],
    );

    blocTest<SensorBloc, SensorState>(
      'clears errorMessage when new realtime data is received',
      seed: () => SensorLoaded(
        historyReadings: [testReading1],
        latestReading: testReading1,
        realtimeBuffer: [testReading1],
        errorMessage: 'Timeout',
      ),
      build: () => SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      ),
      act: (bloc) => bloc.add(SensorRealtimeDataReceived(reading: testReading2)),
      expect: () => [
        SensorLoaded(
          historyReadings: [testReading1],
          latestReading: testReading2,
          realtimeBuffer: [testReading1, testReading2],
          errorMessage: null,
        ),
      ],
    );

    blocTest<SensorBloc, SensorState>(
      'emits SensorError when realtime fails while not loaded yet',
      build: () => SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      ),
      act: (bloc) => bloc.add(const SensorRealtimeFailed(message: 'No connection')),
      expect: () => [
        const SensorError(message: 'No connection'),
      ],
    );
  });

  group('SensorBloc - SensorResetRequested', () {
    test('cancels active realtime polling and resets state to SensorInitial', () async {
      final controller = StreamController<SensorReading>();
      when(() => mockGetRealtimeReadings('ESP_001'))
          .thenAnswer((_) => controller.stream);

      final bloc = SensorBloc(
        getSensorHistory: mockGetSensorHistory,
        getRealtimeReadings: mockGetRealtimeReadings,
      );

      bloc.add(const SensorRealtimeStarted(deviceId: 'ESP_001'));
      await pumpEventQueue();

      expect(controller.hasListener, isTrue);

      bloc.add(const SensorResetRequested());
      await pumpEventQueue();

      expect(bloc.state, equals(const SensorInitial()));
      expect(controller.hasListener, isFalse);

      await bloc.close();
      await controller.close();
    });
  });
}
