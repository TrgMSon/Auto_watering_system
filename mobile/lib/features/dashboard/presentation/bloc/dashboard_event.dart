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

class DashboardPumpToggled extends DashboardEvent {
  final bool turnOn;
  const DashboardPumpToggled(this.turnOn);

  @override
  List<Object?> get props => [turnOn];
}
