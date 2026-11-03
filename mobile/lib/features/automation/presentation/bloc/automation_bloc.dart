import 'package:flutter_bloc/flutter_bloc.dart';
import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';
import '../../domain/repositories/i_automation_repository.dart';
import 'automation_event.dart';
import 'automation_state.dart';

class AutomationBloc extends Bloc<AutomationEvent, AutomationState> {
  final IAutomationRepository repository;

  /// Tăng khi đổi thiết bị hoặc reset, để bỏ qua kết quả API của lần trước.
  int _activeScopeId = 0;

  AutomationBloc({required this.repository}) : super(const AutomationInitial()) {
    on<AutomationLoadRequested>(_onLoadRequested);
    on<AutomationConfigUpdated>(_onConfigUpdated);
    on<AutomationScheduleAdded>(_onScheduleAdded);
    on<AutomationScheduleUpdated>(_onScheduleUpdated);
    on<AutomationScheduleDeleted>(_onScheduleDeleted);
    on<AutomationResetRequested>(_onResetRequested);
  }

  void _onResetRequested(
    AutomationResetRequested event,
    Emitter<AutomationState> emit,
  ) {
    _activeScopeId++;
    emit(const AutomationInitial());
  }

  Future<void> _onLoadRequested(
    AutomationLoadRequested event,
    Emitter<AutomationState> emit,
  ) async {
    final scopeId = ++_activeScopeId;
    emit(const AutomationLoading());
    try {
      final config = await repository.getDeviceConfig(event.deviceId);
      final schedules = await repository.getSchedules(event.deviceId);
      if (scopeId != _activeScopeId) return;
      emit(AutomationLoaded(config: config, schedules: schedules));
    } catch (e) {
      if (scopeId != _activeScopeId) return;
      emit(AutomationError(e.toString()));
    }
  }

  Future<void> _onConfigUpdated(
    AutomationConfigUpdated event,
    Emitter<AutomationState> emit,
  ) async {
    final currentState = state;
    if (currentState is! AutomationLoaded) return;

    final scopeId = _activeScopeId;
    emit(currentState.copyWith(
      config: event.config,
      isSaving: true,
      clearError: true,
    ));

    try {
      await repository.updateDeviceConfig(event.config);
      _finishSaving(scopeId, emit);
    } catch (e) {
      _finishSaving(
        scopeId,
        emit,
        config: currentState.config,
        errorMessage: e.toString(),
      );
    }
  }

  Future<void> _onScheduleAdded(
    AutomationScheduleAdded event,
    Emitter<AutomationState> emit,
  ) =>
      _changeSchedules(
        event.schedule.deviceId,
        () => repository.addSchedule(event.schedule),
        emit,
      );

  Future<void> _onScheduleUpdated(
    AutomationScheduleUpdated event,
    Emitter<AutomationState> emit,
  ) =>
      _changeSchedules(
        event.schedule.deviceId,
        () => repository.updateSchedule(event.schedule),
        emit,
      );

  Future<void> _onScheduleDeleted(
    AutomationScheduleDeleted event,
    Emitter<AutomationState> emit,
  ) =>
      _changeSchedules(
        event.deviceId,
        () => repository.deleteSchedule(event.deviceId, event.scheduleId),
        emit,
      );

  Future<void> _changeSchedules(
    String deviceId,
    Future<bool> Function() change,
    Emitter<AutomationState> emit,
  ) async {
    final currentState = state;
    if (currentState is! AutomationLoaded) return;

    final scopeId = _activeScopeId;
    emit(currentState.copyWith(isSaving: true, clearError: true));

    try {
      await change();
      final schedules = await repository.getSchedules(deviceId);
      _finishSaving(scopeId, emit, schedules: schedules);
    } catch (e) {
      _finishSaving(scopeId, emit, errorMessage: e.toString());
    }
  }

  void _finishSaving(
    int scopeId,
    Emitter<AutomationState> emit, {
    DeviceConfigModel? config,
    List<ScheduleModel>? schedules,
    String? errorMessage,
  }) {
    final latestState = state;
    if (scopeId != _activeScopeId || latestState is! AutomationLoaded) return;
    emit(latestState.copyWith(
      config: config,
      schedules: schedules,
      isSaving: false,
      errorMessage: errorMessage,
      clearError: errorMessage == null,
    ));
  }
}
