import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/entities/statistics_point.dart';
import '../../domain/repositories/sensor_repository.dart';

sealed class StatisticsEvent extends Equatable {
  const StatisticsEvent();

  @override
  List<Object?> get props => [];
}

class StatisticsRequested extends StatisticsEvent {
  final String deviceId;
  final StatisticsRange range;

  const StatisticsRequested({required this.deviceId, required this.range});

  @override
  List<Object?> get props => [deviceId, range];
}

class StatisticsResetRequested extends StatisticsEvent {
  const StatisticsResetRequested();
}

class StatisticsState extends Equatable {
  final StatisticsRange range;
  final bool isLoading;
  final List<StatisticsPoint> points;
  final String? errorMessage;

  const StatisticsState({
    this.range = StatisticsRange.day,
    this.isLoading = false,
    this.points = const [],
    this.errorMessage,
  });

  @override
  List<Object?> get props => [range, isLoading, points, errorMessage];
}

class StatisticsBloc extends Bloc<StatisticsEvent, StatisticsState> {
  final SensorRepository repository;
  int _activeRequestId = 0;

  StatisticsBloc({required this.repository}) : super(const StatisticsState()) {
    on<StatisticsRequested>(_onRequested);
    on<StatisticsResetRequested>(_onResetRequested);
  }

  Future<void> _onRequested(
    StatisticsRequested event,
    Emitter<StatisticsState> emit,
  ) async {
    final requestId = ++_activeRequestId;
    emit(StatisticsState(range: event.range, isLoading: true));
    try {
      final points = await repository.getStatistics(
        deviceId: event.deviceId,
        range: event.range,
      );
      if (requestId != _activeRequestId) return;
      emit(StatisticsState(range: event.range, points: points));
    } catch (e) {
      if (requestId != _activeRequestId) return;
      emit(StatisticsState(range: event.range, errorMessage: e.toString()));
    }
  }

  void _onResetRequested(
    StatisticsResetRequested event,
    Emitter<StatisticsState> emit,
  ) {
    _activeRequestId++;
    emit(const StatisticsState());
  }
}
