import 'package:equatable/equatable.dart';
import '../../data/models/device_model.dart';
import '../../data/models/telemetry_model.dart';

sealed class DashboardState extends Equatable {
  const DashboardState();

  @override
  List<Object?> get props => [];
}

class DashboardInitial extends DashboardState {
  const DashboardInitial();
}

class DashboardLoading extends DashboardState {
  const DashboardLoading();
}

class DashboardLoaded extends DashboardState {
  final List<DeviceModel> devices;
  final DeviceModel? selectedDevice;
  final TelemetryModel? telemetry;
  final bool isTelemetryEmpty;
  final bool isTogglingPump;
  final bool isBleConnected;
  final bool? pumpStatus;
  final DateTime? pumpOffAt;
  final String? errorMessage;
  final String? pumpErrorMessage;
  final bool isRefreshing;

  /// Chế độ tưới tự động của thiết bị đang chọn; null khi chưa tải được.
  final bool? isAutoMode;

  const DashboardLoaded({
    required this.devices,
    this.selectedDevice,
    this.telemetry,
    this.isTelemetryEmpty = false,
    this.isTogglingPump = false,
    this.isBleConnected = false,
    this.pumpStatus,
    this.pumpOffAt,
    this.errorMessage,
    this.pumpErrorMessage,
    this.isRefreshing = false,
    this.isAutoMode,
  });

  DashboardLoaded copyWith({
    List<DeviceModel>? devices,
    DeviceModel? selectedDevice,
    TelemetryModel? telemetry,
    bool clearTelemetry = false,
    bool? isTelemetryEmpty,
    bool? isTogglingPump,
    bool? isBleConnected,
    bool? pumpStatus,
    bool clearPumpStatus = false,
    DateTime? pumpOffAt,
    bool clearPumpOffAt = false,
    String? errorMessage,
    bool clearError = false,
    String? pumpErrorMessage,
    bool clearPumpError = false,
    bool? isRefreshing,
    bool? isAutoMode,
    bool clearAutoMode = false,
  }) {
    return DashboardLoaded(
      devices: devices ?? this.devices,
      selectedDevice: selectedDevice ?? this.selectedDevice,
      telemetry: clearTelemetry ? null : (telemetry ?? this.telemetry),
      isTelemetryEmpty: isTelemetryEmpty ?? (clearTelemetry ? false : this.isTelemetryEmpty),
      isTogglingPump: isTogglingPump ?? this.isTogglingPump,
      isBleConnected: isBleConnected ?? this.isBleConnected,
      pumpStatus: clearPumpStatus ? null : (pumpStatus ?? this.pumpStatus),
      pumpOffAt: clearPumpOffAt ? null : (pumpOffAt ?? this.pumpOffAt),
      errorMessage: clearError ? null : (errorMessage ?? this.errorMessage),
      pumpErrorMessage: clearPumpError ? null : (pumpErrorMessage ?? this.pumpErrorMessage),
      isRefreshing: isRefreshing ?? this.isRefreshing,
      isAutoMode: clearAutoMode ? null : (isAutoMode ?? this.isAutoMode),
    );
  }

  @override
  List<Object?> get props => [
        devices,
        selectedDevice,
        telemetry,
        isTelemetryEmpty,
        isTogglingPump,
        isBleConnected,
        pumpStatus,
        pumpOffAt,
        errorMessage,
        pumpErrorMessage,
        isRefreshing,
        isAutoMode,
      ];
}

class DashboardError extends DashboardState {
  final String message;
  const DashboardError({required this.message});

  @override
  List<Object?> get props => [message];
}
