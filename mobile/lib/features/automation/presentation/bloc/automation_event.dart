import 'package:equatable/equatable.dart';
import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';

sealed class AutomationEvent extends Equatable {
  const AutomationEvent();

  @override
  List<Object?> get props => [];
}

class AutomationLoadRequested extends AutomationEvent {
  final String deviceId;
  final bool loadFromBackend;
  const AutomationLoadRequested(this.deviceId, {this.loadFromBackend = true});

  @override
  List<Object?> get props => [deviceId, loadFromBackend];
}

class AutomationConfigUpdated extends AutomationEvent {
  final DeviceConfigModel config;
  final bool persist;
  const AutomationConfigUpdated(this.config, {this.persist = true});

  @override
  List<Object?> get props => [config, persist];
}

class AutomationModeUpdated extends AutomationEvent {
  final String deviceId;
  final bool automatic;

  const AutomationModeUpdated({required this.deviceId, required this.automatic});

  @override
  List<Object?> get props => [deviceId, automatic];
}

class AutomationScheduleAdded extends AutomationEvent {
  final ScheduleModel schedule;
  const AutomationScheduleAdded(this.schedule);

  @override
  List<Object?> get props => [schedule];
}

class AutomationScheduleUpdated extends AutomationEvent {
  final ScheduleModel schedule;
  const AutomationScheduleUpdated(this.schedule);

  @override
  List<Object?> get props => [schedule];
}

class AutomationScheduleDeleted extends AutomationEvent {
  final int scheduleId;
  final String deviceId;

  const AutomationScheduleDeleted(
      {required this.scheduleId, required this.deviceId});

  @override
  List<Object?> get props => [scheduleId, deviceId];
}

class AutomationResetRequested extends AutomationEvent {
  const AutomationResetRequested();
}
