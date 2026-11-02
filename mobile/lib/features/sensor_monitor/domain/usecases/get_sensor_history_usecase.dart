import '../entities/sensor_reading.dart';
import '../repositories/sensor_repository.dart';

class GetSensorHistoryUseCase {
  final SensorRepository repository;
  const GetSensorHistoryUseCase(this.repository);

  Future<List<SensorReading>> call({
    required String deviceId,
    required DateTime from,
    required DateTime to,
  }) =>
      repository.getSensorHistory(
        deviceId: deviceId,
        from: from,
        to: to,
      );
}
