import 'package:equatable/equatable.dart';
import '../../data/models/device_model.dart';
import '../../data/models/telemetry_model.dart';

sealed class DashboardEvent extends Equatable {
  const DashboardEvent();

  @override
  List<Object?> get props => [];
}

class DashboardLoadRequested extends DashboardEvent {
  const DashboardLoadRequested();
}

class DashboardDeviceSelected extends DashboardEvent {
  final DeviceModel device;
  const DashboardDeviceSelected(this.device);

  @override
  List<Object?> get props => [device];
}

class DashboardTelemetryUpdated extends DashboardEvent {
  final TelemetryModel telemetry;
  const DashboardTelemetryUpdated(this.telemetry);

  @override
  List<Object?> get props => [telemetry];
}

class DashboardTelemetryEmpty extends DashboardEvent {
  const DashboardTelemetryEmpty();
}

class DashboardTelemetryFailed extends DashboardEvent {
  final String message;
  const DashboardTelemetryFailed(this.message);

  @override
  List<Object?> get props => [message];
}

class DashboardPumpToggled extends DashboardEvent {
  final bool turnOn;
  final int? durationSeconds;

  const DashboardPumpToggled(this.turnOn, {this.durationSeconds});

  @override
  List<Object?> get props => [turnOn, durationSeconds];
}

/// Chế độ tự động vừa tải từ server hoặc vừa đổi ở tab Tự động hóa.
class DashboardAutoModeLoaded extends DashboardEvent {
  final String deviceId;
  final bool isAutoMode;

  const DashboardAutoModeLoaded({required this.deviceId, required this.isAutoMode});

  @override
  List<Object?> get props => [deviceId, isAutoMode];
}

class DashboardResetRequested extends DashboardEvent {
  const DashboardResetRequested();
}

class DashboardPumpStatusReceived extends DashboardEvent {
  final String deviceId;
  final bool isOn;

  const DashboardPumpStatusReceived({required this.deviceId, required this.isOn});

  @override
  List<Object?> get props => [deviceId, isOn];
}
