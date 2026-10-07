import 'dart:async';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/repositories/dashboard_repository.dart';
import 'dashboard_event.dart';
import 'dashboard_state.dart';
import '../../data/models/device_model.dart';
import '../../data/models/telemetry_model.dart';

class DashboardBloc extends Bloc<DashboardEvent, DashboardState> {
  final DashboardRepository repository;
  StreamSubscription<TelemetryModel>? _telemetrySubscription;

  DashboardBloc({required this.repository}) : super(const DashboardInitial()) {
    on<DashboardLoadRequested>(_onLoadRequested);
    on<DashboardDeviceSelected>(_onDeviceSelected);
    on<DashboardTelemetryUpdated>(_onTelemetryUpdated);
    on<DashboardPumpToggled>(_onPumpToggled);
  }

  Future<void> _onLoadRequested(
    DashboardLoadRequested event,
    Emitter<DashboardState> emit,
  ) async {
    emit(const DashboardLoading());
    try {
      final devices = await repository.getDevices();
      if (devices.isNotEmpty) {
        emit(DashboardLoaded(devices: devices, selectedDevice: devices.first));
        _listenToTelemetry(devices.first);
      } else {
        emit(const DashboardLoaded(devices: []));
      }
    } catch (e) {
      emit(DashboardError(message: e.toString()));
    }
  }

  void _onDeviceSelected(
    DashboardDeviceSelected event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      emit(currentState.copyWith(selectedDevice: event.device, clearTelemetry: true));
      _listenToTelemetry(event.device);
    }
  }

  void _onTelemetryUpdated(
    DashboardTelemetryUpdated event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      emit(currentState.copyWith(telemetry: event.telemetry));
    }
  }

  Future<void> _onPumpToggled(
    DashboardPumpToggled event,
    Emitter<DashboardState> emit,
  ) async {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      if (currentState.selectedDevice == null) return;
      
      emit(currentState.copyWith(isTogglingPump: true));
      try {
        await repository.togglePump(currentState.selectedDevice!.id, event.turnOn);
        // We don't artificially flip the pump status here, we wait for the stream to update it
        emit(currentState.copyWith(isTogglingPump: false));
      } catch (e) {
        emit(currentState.copyWith(isTogglingPump: false));
      }
    }
  }

  void _listenToTelemetry(DeviceModel device) {
    _telemetrySubscription?.cancel();
    _telemetrySubscription = repository
        .watchTelemetry(device.id, device.blePassKey)
        .listen((telemetry) {
      add(DashboardTelemetryUpdated(telemetry));
    });
  }

  @override
  Future<void> close() {
    _telemetrySubscription?.cancel();
    return super.close();
  }
}
