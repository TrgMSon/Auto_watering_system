import 'package:equatable/equatable.dart';

enum StatisticsRange {
  day('24h', '24 giờ'),
  week('7d', '7 ngày'),
  month('30d', '30 ngày');

  const StatisticsRange(this.apiValue, this.label);

  final String apiValue;
  final String label;
}

/// Giá trị trung bình trong một giờ. Giá trị null nghĩa là giờ đó không có số liệu.
class StatisticsPoint extends Equatable {
  final DateTime time;
  final double? soilMoisture;
  final double? temperature;
  final double? humidity;
  final double? waterLevel;

  const StatisticsPoint({
    required this.time,
    this.soilMoisture,
    this.temperature,
    this.humidity,
    this.waterLevel,
  });

  @override
  List<Object?> get props => [time, soilMoisture, temperature, humidity, waterLevel];
}
