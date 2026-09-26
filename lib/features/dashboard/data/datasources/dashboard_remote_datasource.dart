import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/network/api_client.dart';
import '../models/dashboard_summary_model.dart';

abstract class DashboardRemoteDataSource {
  Future<DashboardSummaryModel> getDashboardSummary();
  Future<bool> toggleAutoWatering({required bool enabled});
}

class DashboardRemoteDataSourceImpl implements DashboardRemoteDataSource {
  final ApiClient apiClient;

  const DashboardRemoteDataSourceImpl({required this.apiClient});

  @override
  Future<DashboardSummaryModel> getDashboardSummary() async {
    try {
      final response = await apiClient.dio.get(ApiEndpoints.dashboardSummary);
      return DashboardSummaryModel.fromJson(response.data);
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 t\u1ea3i d\u1eef li\u1ec7u dashboard: \$e');
    }
  }

  @override
  Future<bool> toggleAutoWatering({required bool enabled}) async {
    try {
      final response = await apiClient.dio.post(
        ApiEndpoints.toggleAutoWatering,
        data: {'enabled': enabled},
      );
      return response.statusCode == 200;
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 chuy\u1ec3n \u0111\u1ed5i ch\u1ebf \u0111\u1ed9 t\u01b0\u1edbi: \$e');
    }
  }
}
