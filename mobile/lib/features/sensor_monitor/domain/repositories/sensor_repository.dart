import '../entities/sensor_reading.dart';
import '../entities/statistics_point.dart';

abstract class SensorRepository {
  Future<List<SensorReading>> getSensorHistory({
    required String deviceId,
    required DateTime from,
    required DateTime to,
  });
  Stream<SensorReading> getRealtimeReadings(String deviceId);
  Future<List<StatisticsPoint>> getStatistics({
    required String deviceId,
    required StatisticsRange range,
  });
}
