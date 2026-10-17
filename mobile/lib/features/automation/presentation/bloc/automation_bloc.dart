import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/repositories/i_automation_repository.dart';
import 'automation_event.dart';
import 'automation_state.dart';

class AutomationBloc extends Bloc<AutomationEvent, AutomationState> {
  final IAutomationRepository repository;

  AutomationBloc({required this.repository}) : super(const AutomationInitial()) {
    on<AutomationLoadRequested>(_onLoadRequested);
    on<AutomationConfigUpdated>(_onConfigUpdated);
    on<AutomationScheduleAdded>(_onScheduleAdded);
    on<AutomationScheduleUpdated>(_onScheduleUpdated);
    on<AutomationScheduleDeleted>(_onScheduleDeleted);
  }

  Future<void> _onLoadRequested(
    AutomationLoadRequested event,
    Emitter<AutomationState> emit,
  ) async {
    emit(const AutomationLoading());
    try {
      final config = await repository.getDeviceConfig(event.deviceId);
      final schedules = await repository.getSchedules(event.deviceId);
      emit(AutomationLoaded(config: config, schedules: schedules));
    } catch (e) {
      emit(AutomationError(e.toString()));
    }
  }

  Future<void> _onConfigUpdated(
    AutomationConfigUpdated event,
    Emitter<AutomationState> emit,
  ) async {
    if (state is AutomationLoaded) {
      final currentState = state as AutomationLoaded;
      
      emit(currentState.copyWith(config: event.config, isSaving: true));
      
      try {
        await repository.updateDeviceConfig(event.config);
        emit(currentState.copyWith(config: event.config, isSaving: false));
      } catch (e) {
        emit(currentState.copyWith(isSaving: false));
      }
    }
  }

  Future<void> _onScheduleAdded(
    AutomationScheduleAdded event,
    Emitter<AutomationState> emit,
  ) async {
    if (state is AutomationLoaded) {
      final currentState = state as AutomationLoaded;
      emit(currentState.copyWith(isSaving: true));
      
      try {
        await repository.addSchedule(event.schedule);
        final newSchedules = await repository.getSchedules(event.schedule.deviceId);
        emit(currentState.copyWith(schedules: newSchedules, isSaving: false));
      } catch (e) {
        emit(currentState.copyWith(isSaving: false));
      }
    }
  }

  Future<void> _onScheduleUpdated(
    AutomationScheduleUpdated event,
    Emitter<AutomationState> emit,
  ) async {
    if (state is AutomationLoaded) {
      final currentState = state as AutomationLoaded;
      emit(currentState.copyWith(isSaving: true));
      
      try {
        await repository.updateSchedule(event.schedule);
        final newSchedules = await repository.getSchedules(event.schedule.deviceId);
        emit(currentState.copyWith(schedules: newSchedules, isSaving: false));
      } catch (e) {
        emit(currentState.copyWith(isSaving: false));
      }
    }
  }

  Future<void> _onScheduleDeleted(
    AutomationScheduleDeleted event,
    Emitter<AutomationState> emit,
  ) async {
    if (state is AutomationLoaded) {
      final currentState = state as AutomationLoaded;
      emit(currentState.copyWith(isSaving: true));
      
      try {
        await repository.deleteSchedule(event.scheduleId);
        final newSchedules = await repository.getSchedules(event.deviceId);
        emit(currentState.copyWith(schedules: newSchedules, isSaving: false));
      } catch (e) {
        emit(currentState.copyWith(isSaving: false));
      }
    }
  }
}
