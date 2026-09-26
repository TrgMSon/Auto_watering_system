import 'package:equatable/equatable.dart';

class SensorReading extends Equatable {
  final int id;
  final double soilMoisture;
  final double temperature;
  final double humidity;
  final double lightIntensity;
  final DateTime timestamp;

  const SensorReading({
    required this.id,
    required this.soilMoisture,
    required this.temperature,
    required this.humidity,
    required this.lightIntensity,
    required this.timestamp,
  });

  @override
  List<Object?> get props => [id, soilMoisture, temperature, humidity, lightIntensity, timestamp];
}
