import 'dart:async';
import 'package:flutter_bloc/flutter_bloc.dart';
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
  }

  Future<void> _onHistoryRequested(
    SensorHistoryRequested event,
    Emitter<SensorState> emit,
  ) async {
    emit(const SensorLoading());
    try {
      final readings = await getSensorHistory(from: event.from, to: event.to);
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
    _realtimeSubscription = getRealtimeReadings().listen(
      (reading) => add(SensorRealtimeDataReceived(reading: reading)),
    );
  }

  void _onRealtimeStopped(
    SensorRealtimeStopped event,
    Emitter<SensorState> emit,
  ) {
    _realtimeSubscription?.cancel();
    _realtimeSubscription = null;
  }

  void _onRealtimeDataReceived(
    SensorRealtimeDataReceived event,
    Emitter<SensorState> emit,
  ) {
    final currentState = state;
    if (currentState is SensorLoaded) {
      final buffer = List.of(currentState.realtimeBuffer)..add(event.reading);
      if (buffer.length > _maxBufferSize) {
        buffer.removeAt(0);
      }
      emit(currentState.copyWith(
        latestReading: event.reading,
        realtimeBuffer: buffer,
      ));
    } else {
      emit(SensorLoaded(
        latestReading: event.reading,
        realtimeBuffer: [event.reading],
      ));
    }
  }

  @override
  Future<void> close() {
    _realtimeSubscription?.cancel();
    return super.close();
  }
}
