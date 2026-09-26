import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/network/api_client.dart';
import '../models/sensor_reading_model.dart';

abstract class SensorRemoteDataSource {
  Future<List<SensorReadingModel>> getSensorHistory({
    required DateTime from,
    required DateTime to,
  });
}

class SensorRemoteDataSourceImpl implements SensorRemoteDataSource {
  final ApiClient apiClient;

  const SensorRemoteDataSourceImpl({required this.apiClient});

  @override
  Future<List<SensorReadingModel>> getSensorHistory({
    required DateTime from,
    required DateTime to,
  }) async {
    try {
      final response = await apiClient.dio.get(
        ApiEndpoints.sensorHistory,
        queryParameters: {
          'from': from.toIso8601String(),
          'to': to.toIso8601String(),
        },
      );
      final List<dynamic> data = response.data as List<dynamic>;
      return data
          .map((json) => SensorReadingModel.fromJson(json as Map<String, dynamic>))
          .toList();
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 t\u1ea3i l\u1ecbch s\u1eed c\u1ea3m bi\u1ebfn: \$e');
    }
  }
}
