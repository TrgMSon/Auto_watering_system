import '../entities/sensor_reading.dart';
import '../repositories/sensor_repository.dart';

class GetRealtimeReadingsUseCase {
  final SensorRepository repository;
  const GetRealtimeReadingsUseCase(this.repository);

  Stream<SensorReading> call() => repository.getRealtimeReadings();
}
