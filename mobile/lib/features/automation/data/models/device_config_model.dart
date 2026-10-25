class DeviceConfigModel {
  final String deviceId;
  final bool autoMode;
  
  // Các ngưỡng tự động mới
  final double soilMoistureMin;
  final double soilMoistureMax;
  final double airHumidityMin;
  final double airHumidityMax;
  final double temperatureMin;
  final double temperatureMax;

  final int maxWateringDuration;
  final int waterLevelMin;

  const DeviceConfigModel({
    required this.deviceId,
    required this.autoMode,
    required this.soilMoistureMin,
    required this.soilMoistureMax,
    required this.airHumidityMin,
    required this.airHumidityMax,
    required this.temperatureMin,
    required this.temperatureMax,
    required this.maxWateringDuration,
    this.waterLevelMin = 0,
  });

  factory DeviceConfigModel.fromJson(Map<String, dynamic> json) {
    return DeviceConfigModel(
      deviceId: json['device_id'] ?? '',
      autoMode: json['auto_mode'] == 1 || json['auto_mode'] == true,
      soilMoistureMin: (json['soil_moisture_min'] ?? 40).toDouble(),
      soilMoistureMax: (json['soil_moisture_max'] ?? 80).toDouble(),
      airHumidityMin: (json['air_humidity_min'] ?? 50).toDouble(),
      airHumidityMax: (json['air_humidity_max'] ?? 80).toDouble(),
      temperatureMin: (json['air_temperature_min'] ?? json['temperature_min'] ?? 20).toDouble(),
      temperatureMax: (json['air_temperature_max'] ?? json['temperature_max'] ?? 35).toDouble(),
      maxWateringDuration: json['max_watering_duration'] ?? 120,
      waterLevelMin: json['water_level_min'] ?? 0,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'auto_mode': autoMode,
      'soil_moisture_min': soilMoistureMin,
      'soil_moisture_max': soilMoistureMax,
      'air_humidity_min': airHumidityMin,
      'air_humidity_max': airHumidityMax,
      'air_temperature_min': temperatureMin,
      'air_temperature_max': temperatureMax,
      'max_watering_duration': maxWateringDuration,
      'water_level_min': waterLevelMin,
    };
  }

  DeviceConfigModel copyWith({
    bool? autoMode,
    double? soilMoistureMin,
    double? soilMoistureMax,
    double? airHumidityMin,
    double? airHumidityMax,
    double? temperatureMin,
    double? temperatureMax,
    int? maxWateringDuration,
    int? waterLevelMin,
  }) {
    return DeviceConfigModel(
      deviceId: deviceId,
      autoMode: autoMode ?? this.autoMode,
      soilMoistureMin: soilMoistureMin ?? this.soilMoistureMin,
      soilMoistureMax: soilMoistureMax ?? this.soilMoistureMax,
      airHumidityMin: airHumidityMin ?? this.airHumidityMin,
      airHumidityMax: airHumidityMax ?? this.airHumidityMax,
      temperatureMin: temperatureMin ?? this.temperatureMin,
      temperatureMax: temperatureMax ?? this.temperatureMax,
      maxWateringDuration: maxWateringDuration ?? this.maxWateringDuration,
      waterLevelMin: waterLevelMin ?? this.waterLevelMin,
    );
  }
}
