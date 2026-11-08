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

  /// Thời gian bơm tối đa, tính bằng phút (1 đến 10).
  final int maxWateringDuration;

  /// Ngưỡng mực nước thấp, tính bằng % bồn như firmware (20 đến 100).
  final int waterLevelMin;

  int get maxWateringDurationSeconds => maxWateringDuration * 60;

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
    this.waterLevelMin = 20,
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
      maxWateringDuration: json['max_watering_duration'] ?? 2,
      waterLevelMin: json['water_level_min'] ?? 20,
    );
  }

  /// Đưa giá trị cũ về khoảng backend và firmware nhận, để lưu không bị lỗi 400.
  /// Thời gian lớn hơn 10 là dữ liệu cũ lưu theo giây (ví dụ 120), nên quy về phút.
  static int normalizeDurationMinutes(int value) {
    final minutes = value > 10 ? (value / 60).round() : value;
    return minutes.clamp(1, 10);
  }

  static int normalizeWaterLevelMin(int value) => value.clamp(20, 100);

  Map<String, dynamic> toJson() {
    return {
      'auto_mode': autoMode,
      'soil_moisture_min': soilMoistureMin,
      'soil_moisture_max': soilMoistureMax,
      'air_humidity_min': airHumidityMin,
      'air_humidity_max': airHumidityMax,
      'air_temperature_min': temperatureMin,
      'air_temperature_max': temperatureMax,
      'max_watering_duration': normalizeDurationMinutes(maxWateringDuration),
      'water_level_min': normalizeWaterLevelMin(waterLevelMin),
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
