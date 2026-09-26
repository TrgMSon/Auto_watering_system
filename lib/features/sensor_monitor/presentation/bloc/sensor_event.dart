import 'package:equatable/equatable.dart';
import '../../domain/entities/sensor_reading.dart';

sealed class SensorEvent extends Equatable {
  const SensorEvent();
  @override
  List<Object?> get props => [];
}

class SensorHistoryRequested extends SensorEvent {
  final DateTime from;
  final DateTime to;
  const SensorHistoryRequested({required this.from, required this.to});

  @override
  List<Object?> get props => [from, to];
}

class SensorRealtimeStarted extends SensorEvent {
  const SensorRealtimeStarted();
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
