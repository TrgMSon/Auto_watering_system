import '../../domain/entities/dashboard_summary.dart';

class DashboardSummaryModel extends DashboardSummary {
  const DashboardSummaryModel({
    required super.isAutoWateringEnabled,
    required super.soilMoisture,
    required super.temperature,
    required super.humidity,
    required super.lightIntensity,
    required super.totalDevices,
    required super.onlineDevices,
    super.lastWateredAt,
  });

  factory DashboardSummaryModel.fromJson(Map<String, dynamic> json) {
    return DashboardSummaryModel(
      isAutoWateringEnabled: json['autoWateringEnabled'] as bool? ?? false,
      soilMoisture: (json['soilMoisture'] as num?)?.toDouble() ?? 0.0,
      temperature: (json['temperature'] as num?)?.toDouble() ?? 0.0,
      humidity: (json['humidity'] as num?)?.toDouble() ?? 0.0,
      lightIntensity: (json['lightIntensity'] as num?)?.toDouble() ?? 0.0,
      totalDevices: json['totalDevices'] as int? ?? 0,
      onlineDevices: json['onlineDevices'] as int? ?? 0,
      lastWateredAt: json['lastWateredAt'] != null
          ? DateTime.tryParse(json['lastWateredAt'] as String)
          : null,
    );
  }
}
