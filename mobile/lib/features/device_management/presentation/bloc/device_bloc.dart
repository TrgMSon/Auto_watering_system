import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/usecases/get_devices_usecase.dart';
import '../../domain/usecases/add_device_usecase.dart';
import '../../domain/usecases/delete_device_usecase.dart';
import '../../domain/repositories/device_repository.dart';
import 'device_event.dart';
import 'device_state.dart';

class DeviceBloc extends Bloc<DeviceEvent, DeviceState> {
  final GetDevicesUseCase getDevices;
  final AddDeviceUseCase addDevice;
  final DeleteDeviceUseCase deleteDevice;
  final DeviceRepository deviceRepository;

  DeviceBloc({
    required this.getDevices,
    required this.addDevice,
    required this.deleteDevice,
    required this.deviceRepository,
  }) : super(const DeviceInitial()) {
    on<DeviceLoadRequested>(_onLoadRequested);
    on<DeviceAddRequested>(_onAddRequested);
    on<DeviceDeleteRequested>(_onDeleteRequested);
    on<DeviceToggleRequested>(_onToggleRequested);
  }

  Future<void> _onLoadRequested(
    DeviceLoadRequested event,
    Emitter<DeviceState> emit,
  ) async {
    emit(const DeviceLoading());
    try {
      final devices = await getDevices();
      emit(DeviceLoaded(devices: devices));
    } catch (e) {
      emit(DeviceError(message: e.toString()));
    }
  }

  Future<void> _onAddRequested(
    DeviceAddRequested event,
    Emitter<DeviceState> emit,
  ) async {
    try {
      await addDevice(name: event.name, type: event.type, location: event.location);
      add(const DeviceLoadRequested());
    } catch (e) {
      emit(DeviceError(message: e.toString()));
    }
  }

  Future<void> _onDeleteRequested(
    DeviceDeleteRequested event,
    Emitter<DeviceState> emit,
  ) async {
    try {
      await deleteDevice(event.deviceId);
      add(const DeviceLoadRequested());
    } catch (e) {
      emit(DeviceError(message: e.toString()));
    }
  }

  Future<void> _onToggleRequested(
    DeviceToggleRequested event,
    Emitter<DeviceState> emit,
  ) async {
    try {
      await deviceRepository.toggleDevice(
        id: event.deviceId,
        activate: event.activate,
      );
      add(const DeviceLoadRequested());
    } catch (e) {
      emit(DeviceError(message: e.toString()));
    }
  }
}
