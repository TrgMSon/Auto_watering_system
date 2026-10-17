import 'package:equatable/equatable.dart';

class DashboardSummary extends Equatable {
  final bool isAutoWateringEnabled;
  final double soilMoisture;
  final double temperature;
  final double humidity;
  final double lightIntensity;
  final int totalDevices;
  final int onlineDevices;
  final DateTime? lastWateredAt;

  const DashboardSummary({
    required this.isAutoWateringEnabled,
    required this.soilMoisture,
    required this.temperature,
    required this.humidity,
    required this.lightIntensity,
    required this.totalDevices,
    required this.onlineDevices,
    this.lastWateredAt,
  });

  @override
  List<Object?> get props => [
    isAutoWateringEnabled, soilMoisture, temperature, humidity,
    lightIntensity, totalDevices, onlineDevices, lastWateredAt,
  ];
}
