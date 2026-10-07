import 'package:equatable/equatable.dart';

class TelemetryModel extends Equatable {
  final String deviceId;
  final double soilMoisture;
  final double temperature;
  final double humidity;
  final double waterLevel;
  final bool isPumpOn;
  final DateTime recordedAt;

  const TelemetryModel({
    required this.deviceId,
    required this.soilMoisture,
    required this.temperature,
    required this.humidity,
    required this.waterLevel,
    required this.isPumpOn,
    required this.recordedAt,
  });

  factory TelemetryModel.fromJson(Map<String, dynamic> json) {
    return TelemetryModel(
      deviceId: json['device_id'] ?? json['deviceId'] ?? '',
      soilMoisture: (json['soil_moisture'] ?? json['soilMoisture'] ?? 0).toDouble(),
      temperature: (json['temperature'] ?? 0).toDouble(),
      humidity: (json['humidity'] ?? 0).toDouble(),
      waterLevel: (json['water_level'] ?? json['waterLevel'] ?? 0).toDouble(),
      isPumpOn: json['pump_status'] == 'ON' || json['pump_status'] == true || json['isPumpOn'] == true,
      recordedAt: json['recorded_at'] != null 
          ? DateTime.parse(json['recorded_at']) 
          : DateTime.now(),
    );
  }

  @override
  List<Object?> get props => [
        deviceId,
        soilMoisture,
        temperature,
        humidity,
        waterLevel,
        isPumpOn,
        recordedAt,
      ];
}
