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
  final String? errorMessage;

  const SensorLoaded({
    this.historyReadings = const [],
    this.latestReading,
    this.realtimeBuffer = const [],
    this.errorMessage,
  });

  List<SensorReading> get chartReadings {
    final seen = <dynamic>{};
    final combined = <SensorReading>[];

    for (final reading in [...historyReadings, ...realtimeBuffer]) {
      final key = reading.id != 0 ? reading.id : reading.timestamp;
      if (seen.add(key)) {
        combined.add(reading);
      }
    }

    combined.sort((a, b) => a.timestamp.compareTo(b.timestamp));
    return combined;
  }

  SensorLoaded copyWith({
    List<SensorReading>? historyReadings,
    SensorReading? latestReading,
    List<SensorReading>? realtimeBuffer,
    String? errorMessage,
    bool clearErrorMessage = false,
  }) {
    return SensorLoaded(
      historyReadings: historyReadings ?? this.historyReadings,
      latestReading: latestReading ?? this.latestReading,
      realtimeBuffer: realtimeBuffer ?? this.realtimeBuffer,
      errorMessage:
          clearErrorMessage ? null : (errorMessage ?? this.errorMessage),
    );
  }

  @override
  List<Object?> get props => [
        historyReadings,
        latestReading,
        realtimeBuffer,
        errorMessage,
      ];
}

class SensorError extends SensorState {
  final String message;
  const SensorError({required this.message});

  @override
  List<Object?> get props => [message];
}
