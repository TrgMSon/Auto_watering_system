import 'package:equatable/equatable.dart';

class TelemetryModel extends Equatable {
  final String deviceId;
  // Null khi cảm biến lỗi hoặc chưa đo được; firmware không gửi 0 thay cho lỗi.
  // Firmware hiện luôn gửi mực nước là null vì chưa hiệu chuẩn được đáy bồn.
  final double? soilMoisture;
  final double? temperature;
  final double? humidity;
  final double? waterLevel;
  final bool? isPumpOn;
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
    final pumpStatus = json['pump_status'];
    return TelemetryModel(
      deviceId: json['device_id'] ?? json['deviceId'] ?? '',
      soilMoisture: _number(json['soil_moisture'] ?? json['soilMoisture']),
      temperature: _number(json['temperature']),
      humidity: _number(json['humidity']),
      waterLevel: _number(json['water_level'] ?? json['waterLevel']),
      isPumpOn: pumpStatus == null && json['isPumpOn'] == null
          ? null
          : pumpStatus == 'ON' ||
              pumpStatus == true ||
              json['isPumpOn'] == true,
      recordedAt: json['recorded_at'] != null
          ? DateTime.parse(json['recorded_at'].toString())
          : DateTime.now(),
    );
  }

  TelemetryModel copyWith({bool? isPumpOn}) => TelemetryModel(
        deviceId: deviceId,
        soilMoisture: soilMoisture,
        temperature: temperature,
        humidity: humidity,
        waterLevel: waterLevel,
        isPumpOn: isPumpOn ?? this.isPumpOn,
        recordedAt: recordedAt,
      );

  static double? _number(dynamic value) =>
      value is num ? value.toDouble() : double.tryParse('${value ?? ''}');

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
