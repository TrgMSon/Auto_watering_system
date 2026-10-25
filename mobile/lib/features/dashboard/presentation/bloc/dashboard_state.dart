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
  final bool isTogglingPump;
  final bool isBleConnected;
  final bool? pumpStatus;
  final String? errorMessage;

  const DashboardLoaded({
    required this.devices,
    this.selectedDevice,
    this.telemetry,
    this.isTogglingPump = false,
    this.isBleConnected = false,
    this.pumpStatus,
    this.errorMessage,
  });

  DashboardLoaded copyWith({
    List<DeviceModel>? devices,
    DeviceModel? selectedDevice,
    TelemetryModel? telemetry,
    bool clearTelemetry = false,
    bool? isTogglingPump,
    bool? isBleConnected,
    bool? pumpStatus,
    bool clearPumpStatus = false,
    String? errorMessage,
    bool clearError = false,
  }) {
    return DashboardLoaded(
      devices: devices ?? this.devices,
      selectedDevice: selectedDevice ?? this.selectedDevice,
      telemetry: clearTelemetry ? null : (telemetry ?? this.telemetry),
      isTogglingPump: isTogglingPump ?? this.isTogglingPump,
      isBleConnected: isBleConnected ?? this.isBleConnected,
      pumpStatus: clearPumpStatus ? null : (pumpStatus ?? this.pumpStatus),
      errorMessage: clearError ? null : (errorMessage ?? this.errorMessage),
    );
  }

  @override
  List<Object?> get props => [
        devices,
        selectedDevice,
        telemetry,
        isTogglingPump,
        isBleConnected,
        pumpStatus,
        errorMessage,
      ];
}

class DashboardError extends DashboardState {
  final String message;
  const DashboardError({required this.message});

  @override
  List<Object?> get props => [message];
}
