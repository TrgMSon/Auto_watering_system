import 'package:equatable/equatable.dart';
import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';

sealed class AutomationState extends Equatable {
  const AutomationState();
  
  @override
  List<Object?> get props => [];
}

class AutomationInitial extends AutomationState {
  const AutomationInitial();
}

class AutomationLoading extends AutomationState {
  const AutomationLoading();
}

class AutomationLoaded extends AutomationState {
  final DeviceConfigModel config;
  final List<ScheduleModel> schedules;
  final bool isSaving;

  const AutomationLoaded({
    required this.config,
    required this.schedules,
    this.isSaving = false,
  });

  AutomationLoaded copyWith({
    DeviceConfigModel? config,
    List<ScheduleModel>? schedules,
    bool? isSaving,
  }) {
    return AutomationLoaded(
      config: config ?? this.config,
      schedules: schedules ?? this.schedules,
      isSaving: isSaving ?? this.isSaving,
    );
  }

  @override
  List<Object?> get props => [config, schedules, isSaving];
}

class AutomationError extends AutomationState {
  final String message;
  const AutomationError(this.message);

  @override
  List<Object?> get props => [message];
}
