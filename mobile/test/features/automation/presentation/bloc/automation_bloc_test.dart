import 'dart:async';

import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/automation/data/models/schedule_model.dart';
import 'package:watering_app/features/automation/domain/repositories/i_automation_repository.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_bloc.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_event.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_state.dart';

class MockAutomationRepository extends Mock implements IAutomationRepository {}

void main() {
  late MockAutomationRepository mockRepository;

  const testConfig = DeviceConfigModel(
    deviceId: 'ESP_001',
    autoMode: true,
    soilMoistureMin: 30.0,
    soilMoistureMax: 70.0,
    airHumidityMin: 40.0,
    airHumidityMax: 80.0,
    temperatureMin: 20.0,
    temperatureMax: 35.0,
    waterLevelMin: 10,
  );

  setUp(() {
    mockRepository = MockAutomationRepository();
  });

  group('AutomationBloc', () {
    blocTest<AutomationBloc, AutomationState>(
      'emits [AutomationLoading, AutomationLoaded] on AutomationLoadRequested',
      build: () {
        when(() => mockRepository.getDeviceConfig('ESP_001'))
            .thenAnswer((_) async => testConfig);
        when(() => mockRepository.getSchedules('ESP_001'))
            .thenAnswer((_) async => []);
        return AutomationBloc(repository: mockRepository);
      },
      act: (bloc) => bloc.add(const AutomationLoadRequested('ESP_001')),
      expect: () => [
        const AutomationLoading(),
        const AutomationLoaded(config: testConfig, schedules: []),
      ],
    );

    blocTest<AutomationBloc, AutomationState>(
      'resets state to AutomationInitial on AutomationResetRequested',
      seed: () => const AutomationLoaded(config: testConfig, schedules: []),
      build: () => AutomationBloc(repository: mockRepository),
      act: (bloc) => bloc.add(const AutomationResetRequested()),
      expect: () => [
        const AutomationInitial(),
      ],
    );

    group('khi đổi thiết bị hoặc đăng xuất trong lúc chờ API', () {
      const otherConfig = DeviceConfigModel(
        deviceId: 'ESP_002',
        autoMode: false,
        soilMoistureMin: 35.0,
        soilMoistureMax: 75.0,
        airHumidityMin: 45.0,
        airHumidityMax: 85.0,
        temperatureMin: 18.0,
        temperatureMax: 33.0,
      );
      const savedConfig = DeviceConfigModel(
        deviceId: 'ESP_001',
        autoMode: false,
        soilMoistureMin: 30.0,
        soilMoistureMax: 70.0,
        airHumidityMin: 40.0,
        airHumidityMax: 80.0,
        temperatureMin: 20.0,
        temperatureMax: 35.0,
      );
      const newSchedule = ScheduleModel(
        deviceId: 'ESP_001',
        startTime: '06:00:00',
        endTime: '06:15:00',
        daysOfWeek: 'MON',
        isEnabled: true,
      );

      late AutomationBloc bloc;

      setUp(() {
        when(() => mockRepository.getDeviceConfig('ESP_002'))
            .thenAnswer((_) async => otherConfig);
        when(() => mockRepository.getSchedules(any()))
            .thenAnswer((_) async => const <ScheduleModel>[]);
        bloc = AutomationBloc(repository: mockRepository);
      });

      tearDown(() => bloc.close());

      test('kết quả tải của thiết bị cũ không ghi đè thiết bị mới', () async {
        final firstLoad = Completer<DeviceConfigModel>();
        when(() => mockRepository.getDeviceConfig('ESP_001'))
            .thenAnswer((_) => firstLoad.future);

        bloc.add(const AutomationLoadRequested('ESP_001'));
        await pumpEventQueue();
        bloc.add(const AutomationLoadRequested('ESP_002'));
        await pumpEventQueue();
        firstLoad.complete(testConfig);
        await pumpEventQueue();

        expect(
          bloc.state,
          const AutomationLoaded(config: otherConfig, schedules: []),
        );
      });

      test('kết quả tải về sau khi reset không khôi phục dữ liệu cũ', () async {
        final load = Completer<DeviceConfigModel>();
        when(() => mockRepository.getDeviceConfig('ESP_001'))
            .thenAnswer((_) => load.future);

        bloc.add(const AutomationLoadRequested('ESP_001'));
        await pumpEventQueue();
        bloc.add(const AutomationResetRequested());
        await pumpEventQueue();
        load.complete(testConfig);
        await pumpEventQueue();

        expect(bloc.state, const AutomationInitial());
      });

      test('lưu cấu hình xong sau khi đổi thiết bị không ghi đè thiết bị mới', () async {
        final save = Completer<bool>();
        when(() => mockRepository.updateDeviceConfig(savedConfig))
            .thenAnswer((_) => save.future);
        bloc.emit(const AutomationLoaded(config: testConfig, schedules: []));

        bloc.add(const AutomationConfigUpdated(savedConfig));
        await pumpEventQueue();
        bloc.add(const AutomationLoadRequested('ESP_002'));
        await pumpEventQueue();
        save.complete(true);
        await pumpEventQueue();

        expect(
          bloc.state,
          const AutomationLoaded(config: otherConfig, schedules: []),
        );
      });

      test('thêm lịch xong sau khi đổi thiết bị không ghi đè thiết bị mới', () async {
        final add = Completer<bool>();
        when(() => mockRepository.addSchedule(newSchedule))
            .thenAnswer((_) => add.future);
        bloc.emit(const AutomationLoaded(config: testConfig, schedules: []));

        bloc.add(const AutomationScheduleAdded(newSchedule));
        await pumpEventQueue();
        bloc.add(const AutomationLoadRequested('ESP_002'));
        await pumpEventQueue();
        add.complete(true);
        await pumpEventQueue();

        expect(
          bloc.state,
          const AutomationLoaded(config: otherConfig, schedules: []),
        );
      });

      test('lưu cấu hình lỗi thì trả về cấu hình cũ và báo lỗi', () async {
        when(() => mockRepository.updateDeviceConfig(savedConfig))
            .thenThrow(Exception('Lỗi lưu'));
        bloc.emit(const AutomationLoaded(config: testConfig, schedules: []));

        bloc.add(const AutomationConfigUpdated(savedConfig));
        await pumpEventQueue();

        expect(
          bloc.state,
          const AutomationLoaded(
            config: testConfig,
            schedules: [],
            errorMessage: 'Exception: Lỗi lưu',
          ),
        );
      });
    });
  });
}
