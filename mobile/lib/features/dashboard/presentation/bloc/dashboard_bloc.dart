import 'dart:async';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/repositories/dashboard_repository.dart';
import 'dashboard_event.dart';
import 'dashboard_state.dart';
import '../../data/models/device_model.dart';
import '../../data/models/telemetry_model.dart';

class DashboardBloc extends Bloc<DashboardEvent, DashboardState> {
  /// Firmware tự tắt bơm sau 5 giây và không đọc thời gian gửi xuống.
  static const int firmwarePumpRunSeconds = 5;

  final DashboardRepository repository;
  final DateTime Function() _now;
  final Duration _deviceStatusRefreshInterval;
  StreamSubscription<TelemetryModel?>? _telemetrySubscription;
  Timer? _deviceStatusRefreshTimer;
  bool _isRefreshingDeviceStatuses = false;
  int _activeLoadRequestId = 0;

  DashboardBloc({
    required this.repository,
    DateTime Function()? now,
    Duration deviceStatusRefreshInterval = const Duration(seconds: 10),
  })  : _now = now ?? DateTime.now,
        _deviceStatusRefreshInterval = deviceStatusRefreshInterval,
        super(const DashboardInitial()) {
    on<DashboardLoadRequested>(_onLoadRequested);
    on<DashboardDeviceStatusRefreshRequested>(_onDeviceStatusRefreshRequested);
    on<DashboardDeviceSelected>(_onDeviceSelected);
    on<DashboardTelemetryUpdated>(_onTelemetryUpdated);
    on<DashboardTelemetryEmpty>(_onTelemetryEmpty);
    on<DashboardTelemetryFailed>(_onTelemetryFailed);
    on<DashboardPumpToggled>(_onPumpToggled);
    on<DashboardPumpStatusReceived>(_onPumpStatusReceived);
    on<DashboardResetRequested>(_onResetRequested);
    on<DashboardAutoModeLoaded>(_onAutoModeLoaded);
  }

  Future<void> _onLoadRequested(
    DashboardLoadRequested event,
    Emitter<DashboardState> emit,
  ) async {
    final requestId = ++_activeLoadRequestId;
    if (state is DashboardLoaded) {
      await _refreshDashboard(state as DashboardLoaded, emit, requestId);
    } else {
      await _initialLoadDashboard(emit, requestId);
    }
  }

  Future<void> _initialLoadDashboard(
    Emitter<DashboardState> emit,
    int requestId,
  ) async {
    emit(const DashboardLoading());
    try {
      final devices = await repository.getDevices();
      if (requestId != _activeLoadRequestId) return;
      if (devices.isNotEmpty) {
        final initialDevice = devices.first;
        emit(DashboardLoaded(devices: devices, selectedDevice: initialDevice));
        _listenToTelemetry(initialDevice);
        _startDeviceStatusRefresh();
      } else {
        _stopDeviceStatusRefresh();
        emit(const DashboardLoaded(devices: []));
      }
    } catch (e) {
      if (requestId != _activeLoadRequestId) return;
      emit(DashboardError(message: e.toString()));
    }
  }

  Future<void> _refreshDashboard(
    DashboardLoaded currentState,
    Emitter<DashboardState> emit,
    int requestId,
  ) async {
    emit(currentState.copyWith(isRefreshing: true, clearError: true));
    try {
      final devices = await repository.getDevices();
      if (requestId != _activeLoadRequestId) return;
      final latestState = _currentLoadedState(currentState);
      if (devices.isEmpty) {
        _telemetrySubscription?.cancel();
        _stopDeviceStatusRefresh();
        emit(const DashboardLoaded(devices: []));
        return;
      }
      _handleRefreshedDevices(latestState, devices, emit);
      _startDeviceStatusRefresh();
    } catch (e) {
      if (requestId != _activeLoadRequestId) return;
      emit(_currentLoadedState(currentState).copyWith(
        isRefreshing: false,
        errorMessage: e.toString(),
      ));
    }
  }

  Future<void> _onDeviceStatusRefreshRequested(
    DashboardDeviceStatusRefreshRequested event,
    Emitter<DashboardState> emit,
  ) async {
    final initialState = state;
    if (initialState is! DashboardLoaded ||
        initialState.selectedDevice == null ||
        _isRefreshingDeviceStatuses) {
      return;
    }

    _isRefreshingDeviceStatuses = true;
    final requestId = _activeLoadRequestId;
    try {
      final devices = await repository.getDevices();
      if (isClosed || requestId != _activeLoadRequestId) return;
      final latestState = state;
      if (latestState is! DashboardLoaded ||
          latestState.selectedDevice?.id != initialState.selectedDevice!.id) {
        return;
      }

      final devicesById = {for (final device in devices) device.id: device};
      final updatedDevices = latestState.devices
          .map((device) => devicesById[device.id] ?? device)
          .toList();
      final selectedDevice = devicesById[latestState.selectedDevice!.id];
      if (selectedDevice == null) return;

      emit(latestState.copyWith(
        devices: updatedDevices,
        selectedDevice: selectedDevice,
        clearError: true,
      ));
    } catch (error) {
      if (!isClosed && state is DashboardLoaded) {
        emit((state as DashboardLoaded).copyWith(
          errorMessage: 'Không thể cập nhật trạng thái thiết bị: $error',
        ));
      }
    } finally {
      _isRefreshingDeviceStatuses = false;
    }
  }

  void _startDeviceStatusRefresh() {
    _deviceStatusRefreshTimer ??= Timer.periodic(
      _deviceStatusRefreshInterval,
      (_) => add(const DashboardDeviceStatusRefreshRequested()),
    );
  }

  void _stopDeviceStatusRefresh() {
    _deviceStatusRefreshTimer?.cancel();
    _deviceStatusRefreshTimer = null;
  }

  DashboardLoaded _currentLoadedState(DashboardLoaded fallbackState) {
    return state is DashboardLoaded ? (state as DashboardLoaded) : fallbackState;
  }

  void _handleRefreshedDevices(
    DashboardLoaded currentState,
    List<DeviceModel> devices,
    Emitter<DashboardState> emit,
  ) {
    final currentSelectedId = currentState.selectedDevice?.id;
    final matchedDevice = devices.cast<DeviceModel?>().firstWhere(
      (d) => d?.id == currentSelectedId,
      orElse: () => null,
    );

    if (matchedDevice != null) {
      emit(currentState.copyWith(
        devices: devices,
        selectedDevice: matchedDevice,
        isRefreshing: false,
      ));
      _loadAutoMode(matchedDevice);
      return;
    }

    final newSelectedDevice = devices.first;
    emit(currentState.copyWith(
      devices: devices,
      selectedDevice: newSelectedDevice,
      clearTelemetry: true,
      isTelemetryEmpty: false,
      clearPumpStatus: true,
      clearPumpOffAt: true,
      clearAutoMode: true,
      isRefreshing: false,
    ));
    _listenToTelemetry(newSelectedDevice);
  }

  void _onDeviceSelected(
    DashboardDeviceSelected event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      emit(currentState.copyWith(
        selectedDevice: event.device,
        clearTelemetry: true,
        isTelemetryEmpty: false,
        clearPumpStatus: true,
        clearPumpOffAt: true,
        clearAutoMode: true,
      ));
      _listenToTelemetry(event.device);
    }
  }

  void _onTelemetryUpdated(
    DashboardTelemetryUpdated event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      final isPumpTurnedOff = event.telemetry.isPumpOn == false;
      emit(currentState.copyWith(
        telemetry: event.telemetry,
        isTelemetryEmpty: false,
        pumpStatus: event.telemetry.isPumpOn,
        clearPumpOffAt: isPumpTurnedOff,
        clearError: true,
      ));
    }
  }

  void _onTelemetryEmpty(
    DashboardTelemetryEmpty event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      final currentState = state as DashboardLoaded;
      emit(currentState.copyWith(
        clearTelemetry: true,
        isTelemetryEmpty: true,
        clearError: true,
      ));
    }
  }

  void _onTelemetryFailed(
    DashboardTelemetryFailed event,
    Emitter<DashboardState> emit,
  ) {
    if (state is DashboardLoaded) {
      emit((state as DashboardLoaded).copyWith(errorMessage: event.message));
    }
  }

  void _onPumpStatusReceived(
    DashboardPumpStatusReceived event,
    Emitter<DashboardState> emit,
  ) {
    repository.applyPumpStatus(event.deviceId, isOn: event.isOn);
    final currentState = state;
    if (currentState is! DashboardLoaded ||
        currentState.selectedDevice?.id != event.deviceId) {
      return;
    }
    emit(currentState.copyWith(
      pumpStatus: event.isOn,
      clearPumpOffAt: !event.isOn,
    ));
  }

  Future<void> _onPumpToggled(
    DashboardPumpToggled event,
    Emitter<DashboardState> emit,
  ) async {
    if (state is! DashboardLoaded) return;
    final currentState = state as DashboardLoaded;
    final targetDevice = currentState.selectedDevice;
    if (targetDevice == null) return;

    final duration = event.turnOn ? (event.durationSeconds ?? firmwarePumpRunSeconds) : 0;
    emit(currentState.copyWith(isTogglingPump: true, clearPumpError: true));
    try {
      await repository.togglePump(targetDevice.id, event.turnOn, duration: duration);
      _handlePumpSuccess(targetDevice, event.turnOn, duration, currentState, emit);
    } catch (e) {
      _handlePumpFailure(e, currentState, emit);
    }
  }

  void _handlePumpSuccess(
    DeviceModel targetDevice,
    bool turnOn,
    int duration,
    DashboardLoaded fallbackState,
    Emitter<DashboardState> emit,
  ) {
    final latestState = _currentLoadedState(fallbackState);
    if (latestState.selectedDevice?.id != targetDevice.id) {
      emit(latestState.copyWith(isTogglingPump: false));
      return;
    }
    final calculatedPumpOffAt = turnOn
        ? _now().add(Duration(seconds: duration))
        : null;
    emit(latestState.copyWith(
      isTogglingPump: false,
      pumpStatus: turnOn,
      pumpOffAt: calculatedPumpOffAt,
      clearPumpOffAt: !turnOn,
    ));
  }

  void _handlePumpFailure(
    Object error,
    DashboardLoaded fallbackState,
    Emitter<DashboardState> emit,
  ) {
    final latestState = _currentLoadedState(fallbackState);
    emit(latestState.copyWith(
      isTogglingPump: false,
      pumpErrorMessage: error.toString(),
    ));
  }

  void _onAutoModeLoaded(
    DashboardAutoModeLoaded event,
    Emitter<DashboardState> emit,
  ) {
    final currentState = state;
    if (currentState is! DashboardLoaded ||
        currentState.selectedDevice?.id != event.deviceId) {
      return;
    }
    emit(currentState.copyWith(isAutoMode: event.isAutoMode));
  }

  /// Lỗi khi tải chỉ bỏ qua: app không khóa nút bơm nếu không biết chế độ.
  Future<void> _loadAutoMode(DeviceModel device) async {
    try {
      final isAutoMode = await repository.getAutoMode(device.id);
      if (isClosed) return;
      add(DashboardAutoModeLoaded(deviceId: device.id, isAutoMode: isAutoMode));
    } catch (_) {}
  }

  void _listenToTelemetry(DeviceModel device) {
    _loadAutoMode(device);
    _telemetrySubscription?.cancel();
    _telemetrySubscription = repository
        .watchTelemetry(device.id, device.blePassKey)
        .listen((telemetry) {
      if (telemetry == null) {
        add(const DashboardTelemetryEmpty());
      } else {
        add(DashboardTelemetryUpdated(telemetry));
      }
    }, onError: (Object error) {
      add(DashboardTelemetryFailed(error.toString()));
    });
  }

  void _onResetRequested(
    DashboardResetRequested event,
    Emitter<DashboardState> emit,
  ) {
    _activeLoadRequestId++;
    _stopDeviceStatusRefresh();
    _telemetrySubscription?.cancel();
    _telemetrySubscription = null;
    emit(const DashboardInitial());
  }

  @override
  Future<void> close() {
    _activeLoadRequestId++;
    _stopDeviceStatusRefresh();
    _telemetrySubscription?.cancel();
    return super.close();
  }
}
