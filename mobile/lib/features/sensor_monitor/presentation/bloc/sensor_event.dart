import 'package:equatable/equatable.dart';
import '../../domain/entities/sensor_reading.dart';

sealed class SensorEvent extends Equatable {
  const SensorEvent();
  @override
  List<Object?> get props => [];
}

class SensorHistoryRequested extends SensorEvent {
  final String deviceId;
  final DateTime from;
  final DateTime to;

  const SensorHistoryRequested({
    required this.deviceId,
    required this.from,
    required this.to,
  });

  @override
  List<Object?> get props => [deviceId, from, to];
}

class SensorRealtimeStarted extends SensorEvent {
  final String deviceId;

  const SensorRealtimeStarted({required this.deviceId});

  @override
  List<Object?> get props => [deviceId];
}

class SensorRealtimeStopped extends SensorEvent {
  const SensorRealtimeStopped();
}

class SensorRealtimeDataReceived extends SensorEvent {
  final SensorReading reading;
  const SensorRealtimeDataReceived({required this.reading});

  @override
  List<Object?> get props => [reading];
}

class SensorRealtimeFailed extends SensorEvent {
  final String message;
  const SensorRealtimeFailed({required this.message});

  @override
  List<Object?> get props => [message];
}
