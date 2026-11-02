import 'dart:async';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/entities/sensor_reading.dart';
import '../../domain/usecases/get_realtime_readings_usecase.dart';
import '../../domain/usecases/get_sensor_history_usecase.dart';
import 'sensor_event.dart';
import 'sensor_state.dart';

class SensorBloc extends Bloc<SensorEvent, SensorState> {
  final GetSensorHistoryUseCase getSensorHistory;
  final GetRealtimeReadingsUseCase getRealtimeReadings;

  StreamSubscription? _realtimeSubscription;
  static const int _maxBufferSize = 50;

  SensorBloc({
    required this.getSensorHistory,
    required this.getRealtimeReadings,
  }) : super(const SensorInitial()) {
    on<SensorHistoryRequested>(_onHistoryRequested);
    on<SensorRealtimeStarted>(_onRealtimeStarted);
    on<SensorRealtimeStopped>(_onRealtimeStopped);
    on<SensorRealtimeDataReceived>(_onRealtimeDataReceived);
    on<SensorRealtimeFailed>(_onRealtimeFailed);
  }

  Future<void> _onHistoryRequested(
    SensorHistoryRequested event,
    Emitter<SensorState> emit,
  ) async {
    emit(const SensorLoading());
    try {
      final readings = await getSensorHistory(
        deviceId: event.deviceId,
        from: event.from,
        to: event.to,
      );
      emit(SensorLoaded(
        historyReadings: readings,
        latestReading: readings.isNotEmpty ? readings.last : null,
      ));
    } catch (e) {
      emit(SensorError(message: e.toString()));
    }
  }

  void _onRealtimeStarted(
    SensorRealtimeStarted event,
    Emitter<SensorState> emit,
  ) {
    _realtimeSubscription?.cancel();
    _realtimeSubscription = getRealtimeReadings(event.deviceId).listen(
      (reading) => add(SensorRealtimeDataReceived(reading: reading)),
      onError: (Object error) =>
          add(SensorRealtimeFailed(message: error.toString())),
    );
  }

  void _onRealtimeStopped(
    SensorRealtimeStopped event,
    Emitter<SensorState> emit,
  ) {
    _realtimeSubscription?.cancel();
    _realtimeSubscription = null;
  }

  void _onRealtimeFailed(
    SensorRealtimeFailed event,
    Emitter<SensorState> emit,
  ) {
    final currentState = state;
    if (currentState is SensorLoaded) {
      emit(currentState.copyWith(errorMessage: event.message));
    } else {
      emit(SensorError(message: event.message));
    }
  }

  void _onRealtimeDataReceived(
    SensorRealtimeDataReceived event,
    Emitter<SensorState> emit,
  ) {
    final currentState = state;
    if (currentState is! SensorLoaded) {
      emit(SensorLoaded(
        latestReading: event.reading,
        realtimeBuffer: [event.reading],
      ));
      return;
    }
    if (currentState.latestReading?.id == event.reading.id) {
      if (currentState.errorMessage != null) {
        emit(currentState.copyWith(clearErrorMessage: true));
      }
      return;
    }
    emit(_appendReading(currentState, event.reading));
  }

  SensorLoaded _appendReading(SensorLoaded current, SensorReading reading) {
    final buffer = List.of(current.realtimeBuffer)..add(reading);
    if (buffer.length > _maxBufferSize) buffer.removeAt(0);
    return current.copyWith(
      latestReading: reading,
      realtimeBuffer: buffer,
      clearErrorMessage: true,
    );
  }

  @override
  Future<void> close() {
    _realtimeSubscription?.cancel();
    return super.close();
  }
}
