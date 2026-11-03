import '../entities/sensor_reading.dart';

abstract class SensorRepository {
  Future<List<SensorReading>> getSensorHistory({
    required String deviceId,
    required DateTime from,
    required DateTime to,
  });
  Stream<SensorReading> getRealtimeReadings(String deviceId);
}
