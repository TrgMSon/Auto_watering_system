import 'package:equatable/equatable.dart';

sealed class DashboardEvent extends Equatable {
  const DashboardEvent();

  @override
  List<Object?> get props => [];
}

class DashboardLoadRequested extends DashboardEvent {
  const DashboardLoadRequested();
}

class DashboardRefreshRequested extends DashboardEvent {
  const DashboardRefreshRequested();
}

class DashboardAutoWateringToggled extends DashboardEvent {
  final bool enabled;
  const DashboardAutoWateringToggled({required this.enabled});

  @override
  List<Object?> get props => [enabled];
}

class DashboardSensorDataUpdated extends DashboardEvent {
  final Map<String, dynamic> sensorData;
  const DashboardSensorDataUpdated({required this.sensorData});

  @override
  List<Object?> get props => [sensorData];
}
