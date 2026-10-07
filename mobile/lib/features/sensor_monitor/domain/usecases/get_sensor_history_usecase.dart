import '../entities/sensor_reading.dart';
import '../repositories/sensor_repository.dart';

class GetSensorHistoryUseCase {
  final SensorRepository repository;
  const GetSensorHistoryUseCase(this.repository);

  Future<List<SensorReading>> call({
    required DateTime from,
    required DateTime to,
  }) => repository.getSensorHistory(from: from, to: to);
}
