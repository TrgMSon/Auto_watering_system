import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/device_management/domain/entities/device.dart';
import 'package:watering_app/features/device_management/domain/repositories/device_repository.dart';
import 'package:watering_app/features/device_management/domain/usecases/add_device_usecase.dart';
import 'package:watering_app/features/device_management/domain/usecases/delete_device_usecase.dart';
import 'package:watering_app/features/device_management/domain/usecases/get_devices_usecase.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_bloc.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_event.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_state.dart';

class MockGetDevicesUseCase extends Mock implements GetDevicesUseCase {}

class MockAddDeviceUseCase extends Mock implements AddDeviceUseCase {}

class MockDeleteDeviceUseCase extends Mock implements DeleteDeviceUseCase {}

class MockDeviceRepository extends Mock implements DeviceRepository {}

void main() {
  late MockGetDevicesUseCase mockGetDevices;
  late MockAddDeviceUseCase mockAddDevice;
  late MockDeleteDeviceUseCase mockDeleteDevice;
  late MockDeviceRepository mockDeviceRepository;

  final testDevice = Device(
    id: '1',
    name: 'Máy bơm vườn',
    type: DeviceType.pump,
    isOnline: true,
  );

  setUp(() {
    mockGetDevices = MockGetDevicesUseCase();
    mockAddDevice = MockAddDeviceUseCase();
    mockDeleteDevice = MockDeleteDeviceUseCase();
    mockDeviceRepository = MockDeviceRepository();
  });

  group('DeviceBloc', () {
    blocTest<DeviceBloc, DeviceState>(
      'emits [DeviceLoading, DeviceLoaded] on DeviceLoadRequested',
      build: () {
        when(() => mockGetDevices()).thenAnswer((_) async => [testDevice]);
        return DeviceBloc(
          getDevices: mockGetDevices,
          addDevice: mockAddDevice,
          deleteDevice: mockDeleteDevice,
          deviceRepository: mockDeviceRepository,
        );
      },
      act: (bloc) => bloc.add(const DeviceLoadRequested()),
      expect: () => [
        const DeviceLoading(),
        DeviceLoaded(devices: [testDevice]),
      ],
    );

    blocTest<DeviceBloc, DeviceState>(
      'resets state to DeviceInitial on DeviceResetRequested',
      seed: () => DeviceLoaded(devices: [testDevice]),
      build: () => DeviceBloc(
        getDevices: mockGetDevices,
        addDevice: mockAddDevice,
        deleteDevice: mockDeleteDevice,
        deviceRepository: mockDeviceRepository,
      ),
      act: (bloc) => bloc.add(const DeviceResetRequested()),
      expect: () => [
        const DeviceInitial(),
      ],
    );
  });
}

