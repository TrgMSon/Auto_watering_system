import '../../domain/entities/sensor_reading.dart';
import '../../domain/repositories/sensor_repository.dart';
import '../datasources/sensor_remote_datasource.dart';
import '../datasources/sensor_websocket_datasource.dart';

class SensorRepositoryImpl implements SensorRepository {
  final SensorRemoteDataSource remoteDataSource;
  final SensorWebSocketDataSource webSocketDataSource;

  const SensorRepositoryImpl({
    required this.remoteDataSource,
    required this.webSocketDataSource,
  });

  @override
  Future<List<SensorReading>> getSensorHistory({
    required DateTime from,
    required DateTime to,
  }) => remoteDataSource.getSensorHistory(from: from, to: to);

  @override
  Stream<SensorReading> getRealtimeReadings() {
    webSocketDataSource.startListening();
    return webSocketDataSource.sensorStream;
  }
}
