import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/entities/dashboard_summary.dart';
import '../../domain/usecases/get_dashboard_summary_usecase.dart';
import '../../domain/usecases/toggle_auto_watering_usecase.dart';
import 'dashboard_event.dart';
import 'dashboard_state.dart';

class DashboardBloc extends Bloc<DashboardEvent, DashboardState> {
  final GetDashboardSummaryUseCase getDashboardSummary;
  final ToggleAutoWateringUseCase toggleAutoWatering;

  DashboardBloc({
    required this.getDashboardSummary,
    required this.toggleAutoWatering,
  }) : super(const DashboardInitial()) {
    on<DashboardLoadRequested>(_onLoadRequested);
    on<DashboardRefreshRequested>(_onRefreshRequested);
    on<DashboardAutoWateringToggled>(_onAutoWateringToggled);
    on<DashboardSensorDataUpdated>(_onSensorDataUpdated);
  }

  Future<void> _onLoadRequested(
    DashboardLoadRequested event,
    Emitter<DashboardState> emit,
  ) async {
    emit(const DashboardLoading());
    try {
      final summary = await getDashboardSummary();
      emit(DashboardLoaded(summary: summary));
    } catch (e) {
      emit(DashboardError(message: e.toString()));
    }
  }

  Future<void> _onRefreshRequested(
    DashboardRefreshRequested event,
    Emitter<DashboardState> emit,
  ) async {
    try {
      final summary = await getDashboardSummary();
      emit(DashboardLoaded(summary: summary));
    } catch (e) {
      emit(DashboardError(message: e.toString()));
    }
  }

  Future<void> _onAutoWateringToggled(
    DashboardAutoWateringToggled event,
    Emitter<DashboardState> emit,
  ) async {
    final currentState = state;
    if (currentState is DashboardLoaded) {
      emit(currentState.copyWith(isTogglingWatering: true));

      try {
        final success = await toggleAutoWatering(enabled: event.enabled);
        if (success) {
          final summary = await getDashboardSummary();
          emit(DashboardLoaded(summary: summary));
        } else {
          emit(currentState.copyWith(isTogglingWatering: false));
        }
      } catch (_) {
        emit(currentState.copyWith(isTogglingWatering: false));
      }
    }
  }

  void _onSensorDataUpdated(
    DashboardSensorDataUpdated event,
    Emitter<DashboardState> emit,
  ) {
    final currentState = state;
    if (currentState is DashboardLoaded) {
      final data = event.sensorData;
      final updatedSummary = DashboardSummary(
        isAutoWateringEnabled: currentState.summary.isAutoWateringEnabled,
        soilMoisture: (data['soilMoisture'] as num?)?.toDouble() ??
            currentState.summary.soilMoisture,
        temperature: (data['temperature'] as num?)?.toDouble() ??
            currentState.summary.temperature,
        humidity: (data['humidity'] as num?)?.toDouble() ??
            currentState.summary.humidity,
        lightIntensity: (data['lightIntensity'] as num?)?.toDouble() ??
            currentState.summary.lightIntensity,
        totalDevices: currentState.summary.totalDevices,
        onlineDevices: currentState.summary.onlineDevices,
        lastWateredAt: currentState.summary.lastWateredAt,
      );
      emit(DashboardLoaded(summary: updatedSummary));
    }
  }
}
