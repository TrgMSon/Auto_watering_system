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
  late StreamController<TelemetryModel?> telemetryController;

  final testDevice1 = DeviceModel(
    id: 'ESP_001',
    deviceName: 'Vườn Lan Ban Công',
    status: 'active',
    permission: 'OWNER',
    blePassKey: '123456',
  );

  final testDevice2 = DeviceModel(
    id: 'ESP_002',
    deviceName: 'Vườn Hồng Sân Thượng',
    status: 'offline',
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
    telemetryController = StreamController<TelemetryModel?>.broadcast();
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
      expect(const DashboardTelemetryEmpty().props, isEmpty);
      expect(const DashboardTelemetryFailed('error').props, ['error']);
      expect(const DashboardPumpToggled(true, durationSeconds: 60).props, [true, 60]);
    });

    test('DashboardLoaded props equality and copyWith isRefreshing', () {
      final initialLoaded = DashboardLoaded(
        devices: [testDevice1],
        isRefreshing: false,
      );
      final refreshingLoaded = initialLoaded.copyWith(
        isRefreshing: true,
        pumpOffAt: DateTime(2026, 11, 2, 12, 0),
        pumpErrorMessage: 'Err',
      );
      expect(refreshingLoaded.isRefreshing, isTrue);
      expect(refreshingLoaded.pumpOffAt, equals(DateTime(2026, 11, 2, 12, 0)));
      expect(refreshingLoaded.pumpErrorMessage, equals('Err'));
      expect(initialLoaded == refreshingLoaded, isFalse);
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

      blocTest<DashboardBloc, DashboardState>(
        'when already DashboardLoaded, keeps second device selected after refresh without re-subscribing',
        build: () {
          when(() => mockRepository.getDevices())
              .thenAnswer((_) async => [testDevice1, testDevice2]);
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice2,
          telemetry: testTelemetry2,
          pumpStatus: true,
        ),
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: true,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: true,
            isRefreshing: false,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
          verifyNever(() => mockRepository.watchTelemetry(any(), any()));
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'when already DashboardLoaded and selected device is removed, falls back to first device and subscribes to telemetry',
        build: () {
          when(() => mockRepository.getDevices())
              .thenAnswer((_) async => [testDevice1]);
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice2,
          telemetry: testTelemetry2,
          pumpStatus: true,
        ),
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: true,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: null,
            pumpStatus: null,
            isTelemetryEmpty: false,
            isRefreshing: false,
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
          verify(() => mockRepository.watchTelemetry(testDevice1.id, testDevice1.blePassKey))
              .called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'when refresh fails, retains existing devices and telemetry and sets errorMessage',
        build: () {
          when(() => mockRepository.getDevices())
              .thenThrow(Exception('Network error during refresh'));
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice2,
          telemetry: testTelemetry2,
          pumpStatus: true,
        ),
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: true,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: true,
            isRefreshing: false,
            errorMessage: 'Exception: Network error during refresh',
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.getDevices()).called(1);
          verifyNever(() => mockRepository.watchTelemetry(any(), any()));
        },
      );

      Completer<List<DeviceModel>>? inFlightGetDevicesCompleter;

      blocTest<DashboardBloc, DashboardState>(
        'when device changes and telemetry arrives while getDevices is awaiting, retains the new device and telemetry after refresh finishes',
        setUp: () {
          inFlightGetDevicesCompleter = Completer<List<DeviceModel>>();
          when(() => mockRepository.getDevices())
              .thenAnswer((_) => inFlightGetDevicesCompleter!.future);
        },
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) async {
          bloc.add(const DashboardLoadRequested());
          await Future<void>.delayed(Duration.zero);
          bloc.add(DashboardDeviceSelected(testDevice2));
          await Future<void>.delayed(Duration.zero);
          bloc.add(DashboardTelemetryUpdated(testTelemetry2));
          await Future<void>.delayed(Duration.zero);
          inFlightGetDevicesCompleter!.complete([testDevice1, testDevice2]);
        },
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: testTelemetry2.isPumpOn,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: testTelemetry2,
            pumpStatus: testTelemetry2.isPumpOn,
            isRefreshing: false,
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'when device changes while getDevices is awaiting and refresh fails, retains the new device and sets errorMessage',
        setUp: () {
          inFlightGetDevicesCompleter = Completer<List<DeviceModel>>();
          when(() => mockRepository.getDevices())
              .thenAnswer((_) => inFlightGetDevicesCompleter!.future);
        },
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) async {
          bloc.add(const DashboardLoadRequested());
          await Future<void>.delayed(Duration.zero);
          bloc.add(DashboardDeviceSelected(testDevice2));
          await Future<void>.delayed(Duration.zero);
          inFlightGetDevicesCompleter!.completeError(Exception('Network error during refresh'));
        },
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
            isRefreshing: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
            isRefreshing: false,
            errorMessage: 'Exception: Network error during refresh',
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'when refresh returns empty device list, emits DashboardLoaded with empty devices list',
        build: () {
          when(() => mockRepository.getDevices()).thenAnswer((_) async => []);
          return DashboardBloc(repository: mockRepository);
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(const DashboardLoadRequested()),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isRefreshing: true,
          ),
          const DashboardLoaded(
            devices: [],
            isRefreshing: false,
          ),
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
            pumpStatus: testTelemetry2.isPumpOn,
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'resets isTelemetryEmpty to false when telemetry is updated',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          isTelemetryEmpty: true,
        ),
        act: (bloc) => bloc.add(DashboardTelemetryUpdated(testTelemetry1)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isTelemetryEmpty: false,
            pumpStatus: testTelemetry1.isPumpOn,
          ),
        ],
      );
    });

    group('DashboardTelemetryEmpty', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits DashboardLoaded with isTelemetryEmpty: true and telemetry: null',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(const DashboardTelemetryEmpty()),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: null,
            isTelemetryEmpty: true,
          ),
        ],
      );
    });

    group('DashboardTelemetryFailed', () {
      blocTest<DashboardBloc, DashboardState>(
        'emits DashboardLoaded with errorMessage when telemetry fails',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
        ),
        act: (bloc) => bloc.add(const DashboardTelemetryFailed('Network error')),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            errorMessage: 'Network error',
          ),
        ],
      );
    });

    group('DashboardPumpToggled', () {
      final fixedNow = DateTime(2026, 11, 2, 12, 0, 0);
      Completer<void>? togglePumpCompleter;

      blocTest<DashboardBloc, DashboardState>(
        'emits [isTogglingPump: true, isTogglingPump: false] with pumpOffAt on success',
        build: () {
          when(() => mockRepository.togglePump(testDevice1.id, true, duration: any(named: 'duration')))
              .thenAnswer((_) async {});
          return DashboardBloc(
            repository: mockRepository,
            now: () => fixedNow,
          );
        },
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) => bloc.add(const DashboardPumpToggled(true, durationSeconds: 60)),
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
            pumpStatus: true,
            pumpOffAt: fixedNow.add(const Duration(seconds: 60)),
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.togglePump(testDevice1.id, true, duration: 60)).called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'resets isTogglingPump to false and sets pumpErrorMessage when togglePump throws exception',
        build: () {
          when(() => mockRepository.togglePump(testDevice1.id, false, duration: any(named: 'duration')))
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
            pumpErrorMessage: 'Exception: Device offline',
          ),
        ],
        verify: (_) {
          verify(() => mockRepository.togglePump(testDevice1.id, false, duration: 0)).called(1);
        },
      );

      blocTest<DashboardBloc, DashboardState>(
        'telemetry received while awaiting togglePump is not lost when togglePump completes',
        setUp: () {
          togglePumpCompleter = Completer<void>();
          when(() => mockRepository.togglePump(testDevice1.id, true, duration: any(named: 'duration')))
              .thenAnswer((_) => togglePumpCompleter!.future);
        },
        build: () => DashboardBloc(
          repository: mockRepository,
          now: () => fixedNow,
        ),
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) async {
          bloc.add(const DashboardPumpToggled(true, durationSeconds: 120));
          await Future<void>.delayed(Duration.zero);
          bloc.add(DashboardTelemetryUpdated(testTelemetry2));
          await Future<void>.delayed(Duration.zero);
          togglePumpCompleter!.complete();
        },
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
            telemetry: testTelemetry2,
            pumpStatus: testTelemetry2.isPumpOn,
            isTogglingPump: true,
          ),
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry2,
            pumpStatus: true,
            pumpOffAt: fixedNow.add(const Duration(seconds: 120)),
            isTogglingPump: false,
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'pumpErrorMessage is not erased when new telemetry arrives',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
          pumpErrorMessage: 'Exception: Command timeout',
        ),
        act: (bloc) => bloc.add(DashboardTelemetryUpdated(testTelemetry2)),
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1],
            selectedDevice: testDevice1,
            telemetry: testTelemetry2,
            pumpStatus: testTelemetry2.isPumpOn,
            pumpErrorMessage: 'Exception: Command timeout',
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'switching device while awaiting togglePump does not overwrite the new device state',
        setUp: () {
          togglePumpCompleter = Completer<void>();
          when(() => mockRepository.togglePump(testDevice1.id, true, duration: any(named: 'duration')))
              .thenAnswer((_) => togglePumpCompleter!.future);
        },
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          telemetry: testTelemetry1,
        ),
        act: (bloc) async {
          bloc.add(const DashboardPumpToggled(true));
          await Future<void>.delayed(Duration.zero);
          bloc.add(DashboardDeviceSelected(testDevice2));
          await Future<void>.delayed(Duration.zero);
          togglePumpCompleter!.complete();
        },
        expect: () => [
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice1,
            telemetry: testTelemetry1,
            isTogglingPump: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
            isTogglingPump: true,
          ),
          DashboardLoaded(
            devices: [testDevice1, testDevice2],
            selectedDevice: testDevice2,
            telemetry: null,
            isTogglingPump: false,
          ),
        ],
      );

      blocTest<DashboardBloc, DashboardState>(
        'does nothing on DashboardPumpToggled when selectedDevice is null',
        build: () => DashboardBloc(repository: mockRepository),
        seed: () => const DashboardLoaded(devices: []),
        act: (bloc) => bloc.add(const DashboardPumpToggled(true)),
        expect: () => [],
        verify: (_) {
          verifyNever(() => mockRepository.togglePump(any(), any(), duration: any(named: 'duration')));
        },
      );
    });

    group('DashboardResetRequested', () {
      test('cancels active telemetry polling and resets state to DashboardInitial', () async {
        when(() => mockRepository.getDevices())
            .thenAnswer((_) async => [testDevice1]);

        final bloc = DashboardBloc(repository: mockRepository);
        bloc.add(const DashboardLoadRequested());
        await pumpEventQueue();

        expect(bloc.state, isA<DashboardLoaded>());
        expect(telemetryController.hasListener, isTrue);

        bloc.add(const DashboardResetRequested());
        await pumpEventQueue();

        expect(bloc.state, equals(const DashboardInitial()));
        expect(telemetryController.hasListener, isFalse);

        await bloc.close();
      });

      test('ignores initial getDevices result if reset occurs while request is pending', () async {
        final completer = Completer<List<DeviceModel>>();
        when(() => mockRepository.getDevices())
            .thenAnswer((_) => completer.future);

        final bloc = DashboardBloc(repository: mockRepository);
        bloc.add(const DashboardLoadRequested());
        await pumpEventQueue();

        expect(bloc.state, isA<DashboardLoading>());

        bloc.add(const DashboardResetRequested());
        await pumpEventQueue();

        expect(bloc.state, equals(const DashboardInitial()));

        completer.complete([testDevice1]);
        await pumpEventQueue();

        expect(bloc.state, equals(const DashboardInitial()));
        expect(telemetryController.hasListener, isFalse);

        await bloc.close();
      });

      test('ignores refreshed getDevices result if reset occurs while refresh is pending', () async {
        final completer = Completer<List<DeviceModel>>();
        when(() => mockRepository.getDevices())
            .thenAnswer((_) => completer.future);

        final bloc = DashboardBloc(repository: mockRepository);
        bloc.emit(DashboardLoaded(devices: [testDevice1], selectedDevice: testDevice1));

        bloc.add(const DashboardLoadRequested());
        await pumpEventQueue();

        expect((bloc.state as DashboardLoaded).isRefreshing, isTrue);

        bloc.add(const DashboardResetRequested());
        await pumpEventQueue();

        expect(bloc.state, equals(const DashboardInitial()));

        completer.complete([testDevice1]);
        await pumpEventQueue();

        expect(bloc.state, equals(const DashboardInitial()));

        await bloc.close();
      });
    });
  });

  group('DashboardPumpStatusReceived', () {
    blocTest<DashboardBloc, DashboardState>(
      'updates the pump of the selected device',
      build: () => DashboardBloc(repository: mockRepository),
      seed: () => DashboardLoaded(
        devices: [testDevice1, testDevice2],
        selectedDevice: testDevice1,
        pumpOffAt: DateTime(2026, 11, 5, 10),
      ),
      act: (bloc) => bloc.add(
        DashboardPumpStatusReceived(deviceId: testDevice1.id, isOn: false),
      ),
      expect: () => [
        DashboardLoaded(
          devices: [testDevice1, testDevice2],
          selectedDevice: testDevice1,
          pumpStatus: false,
        ),
      ],
      verify: (_) {
        verify(() => mockRepository.applyPumpStatus(testDevice1.id, isOn: false)).called(1);
      },
    );

    blocTest<DashboardBloc, DashboardState>(
      'remembers the status of another device without changing the screen',
      build: () => DashboardBloc(repository: mockRepository),
      seed: () => DashboardLoaded(
        devices: [testDevice1, testDevice2],
        selectedDevice: testDevice1,
      ),
      act: (bloc) => bloc.add(
        DashboardPumpStatusReceived(deviceId: testDevice2.id, isOn: true),
      ),
      expect: () => <DashboardState>[],
      verify: (_) {
        verify(() => mockRepository.applyPumpStatus(testDevice2.id, isOn: true)).called(1);
      },
    );
  });
}
