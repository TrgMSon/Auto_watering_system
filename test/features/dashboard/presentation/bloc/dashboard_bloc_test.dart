import 'dart:async';
import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/dashboard/data/models/device_model.dart';
import 'package:watering_app/features/dashboard/data/models/telemetry_model.dart';
import 'package:watering_app/features/dashboard/domain/repositories/dashboard_repository.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_bloc.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_event.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_state.dart';

class MockDashboardRepository extends Mock implements DashboardRepository {}

void main() {
  late MockDashboardRepository mockRepository;
  late StreamController<TelemetryModel> telemetryController;

  final testDevice1 = DeviceModel(
    id: 'ESP_001',
    deviceName: 'Vườn Lan Ban Công',
    status: 'ONLINE',
    permission: 'OWNER',
    blePassKey: '123456',
  );

  final testDevice2 = DeviceModel(
    id: 'ESP_002',
    deviceName: 'Vườn Hồng Sân Thượng',
    status: 'OFFLINE',
    permission: 'VIEWER',
  );

  final testTelemetry1 = TelemetryModel(
    deviceId: 'ESP_001',
    soilMoisture: 45.0,
    temperature: 28.5,
    humidity: 65.0,
    waterLevel: 80.0,
    isPumpOn: false,
    recordedAt: DateTime(2026, 10, 7, 10, 0),
  );

  final testTelemetry2 = TelemetryModel(
    deviceId: 'ESP_001',
    soilMoisture: 50.0,
    temperature: 29.0,
    humidity: 62.0,
    waterLevel: 75.0,
    isPumpOn: true,
    recordedAt: DateTime(2026, 10, 7, 10, 5),
  );

  setUp(() {
    mockRepository = MockDashboardRepository();
    telemetryController = StreamController<TelemetryModel>.broadcast();
    when(() => mockRepository.watchTelemetry(any(), any()))
        .thenAnswer((_) => telemetryController.stream);
  });

  tearDown(() {
    telemetryController.close();
  });

  group('DashboardBloc', () {
    test('initial state is DashboardInitial', () {
      final bloc = DashboardBloc(repository: mockRepository);
      expect(bloc.state, equals(const DashboardInitial()));
      bloc.close();
    });

    test('DashboardEvent props equality', () {
      expect(const DashboardLoadRequested().props, isEmpty);
      expect(DashboardDeviceSelected(testDevice1).props, [testDevice1]);
      expect(DashboardTelemetryUpdated(testTelemetry1).props, [testTelemetry1]);
      expect(const DashboardPumpToggled(true).props, [true]);
    });

    group('DashboardLoadRequested', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits [DashboardLoading, DashboardLoaded] when getDevices succeeds with devices',
        build: () {
          when(() => mockRepository.getDevices())
              .thenAnswer((_) async => [testDevice1, testDevice2]);
          return DashboardBloc(repository: mockRepository);
        },
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          const DashboardLoading(),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
          verify(() => mockRepository.watchTelemetry(testDevice1.id, testDevice1.blePassKey))
              .called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'emits [DashboardLoading, DashboardLoaded(devices: [])] when getDevices returns empty list',
        build: () {
          when(() => mockRepository.getDevices()).thenAnswer((_) async => []);
          return DashboardBloc(repository: mockRepository);
        },
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          const DashboardLoading(),
          const DashboardLoaded(devices: []),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
          verifyNever(() => mockRepository.watchTelemetry(any(), any()));
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'emits [DashboardLoading, DashboardError] when getDevices throws exception',
        build: () {
          when(() => mockRepository.getDevices())
              .thenThrow(Exception('Failed to load devices'));
          return DashboardBloc(repository: mockRepository);
        },
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          const DashboardLoading(),
          const DashboardError(message: 'Exception: Failed to load devices'),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
        },
      );
    });

    group('DashboardDeviceSelected', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits updated selectedDevice with cleared telemetry and subscribes to new telemetry',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(DashboardDeviceSelected(testDevice2)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.watchTelemetry(testDevice2.id, testDevice2.blePassKey))
              .called(1);
        },
      );
    });

    group('DashboardTelemetryUpdated', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits DashboardLoaded with updated telemetry data',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(DashboardTelemetryUpdated(testTelemetry2)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: testTelemetry2,
          ),
        ],
      );
    });

    group('DashboardPumpToggled', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits [isTogglingPump: true, isTogglingPump: false] on success',
        build: () {
          when(() => mockRepository.togglePump(testDevice1.id, true))
              .thenAnswer((_) async {});
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(const DashboardPumpToggled(true)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isTogglingPump: true,
          ),
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isTogglingPump: false,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.togglePump(testDevice1.id, true)).called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'resets isTogglingPump to false when togglePump throws exception',
        build: () {
          when(() => mockRepository.togglePump(testDevice1.id, false))
              .thenThrow(Exception('Device offline'));
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry2,
        ),
        act: (bloc) => bloc.add(const DashboardPumpToggled(false)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry2,
            isTogglingPump: true,
          ),
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry2,
            isTogglingPump: false,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.togglePump(testDevice1.id, false)).called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'does nothing on DashboardPumpToggled when selectedDevice is null',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => const DashboardLoaded(devices: []),
        act: (bloc) => bloc.add(const DashboardPumpToggled(true)),
        expect: () => [],
        verify: (_) {
          verifyNever(() => mockRepository.togglePump(any(), any()));
        },
      );
    });
  });
}
