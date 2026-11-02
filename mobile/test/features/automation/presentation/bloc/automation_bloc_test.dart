import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
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
    maxWateringDuration: 120,
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
  });
}
