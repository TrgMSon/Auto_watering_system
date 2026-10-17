import 'package:equatable/equatable.dart';
import '../../domain/entities/sensor_reading.dart';

sealed class SensorState extends Equatable {
  const SensorState();
  @override
  List<Object?> get props => [];
}

class SensorInitial extends SensorState {
  const SensorInitial();
}

class SensorLoading extends SensorState {
  const SensorLoading();
}

class SensorLoaded extends SensorState {
  final List<SensorReading> historyReadings;
  final SensorReading? latestReading;
  final List<SensorReading> realtimeBuffer;

  const SensorLoaded({
    this.historyReadings = const [],
    this.latestReading,
    this.realtimeBuffer = const [],
  });

  SensorLoaded copyWith({
    List<SensorReading>? historyReadings,
    SensorReading? latestReading,
    List<SensorReading>? realtimeBuffer,
  }) {
    return SensorLoaded(
      historyReadings: historyReadings ?? this.historyReadings,
      latestReading: latestReading ?? this.latestReading,
      realtimeBuffer: realtimeBuffer ?? this.realtimeBuffer,
    );
  }

  @override
  List<Object?> get props => [historyReadings, latestReading, realtimeBuffer];
}

class SensorError extends SensorState {
  final String message;
  const SensorError({required this.message});

  @override
  List<Object?> get props => [message];
}
