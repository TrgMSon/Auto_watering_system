import '../../domain/entities/sensor_reading.dart';

class SensorReadingModel extends SensorReading {
  const SensorReadingModel({
    required super.id,
    required super.soilMoisture,
    required super.temperature,
    required super.humidity,
    required super.lightIntensity,
    required super.timestamp,
  });

  factory SensorReadingModel.fromJson(Map<String, dynamic> json) {
    return SensorReadingModel(
      id: json['id'] as int? ?? 0,
      soilMoisture: (json['soilMoisture'] as num?)?.toDouble() ?? 0.0,
      temperature: (json['temperature'] as num?)?.toDouble() ?? 0.0,
      humidity: (json['humidity'] as num?)?.toDouble() ?? 0.0,
      lightIntensity: (json['lightIntensity'] as num?)?.toDouble() ?? 0.0,
      timestamp: DateTime.parse(json['timestamp'] as String),
    );
  }
}
