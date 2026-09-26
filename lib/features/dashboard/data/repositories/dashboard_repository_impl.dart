import '../../domain/entities/dashboard_summary.dart';
import '../../domain/repositories/dashboard_repository.dart';
import '../datasources/dashboard_remote_datasource.dart';

class DashboardRepositoryImpl implements DashboardRepository {
  final DashboardRemoteDataSource remoteDataSource;

  const DashboardRepositoryImpl({required this.remoteDataSource});

  @override
  Future<DashboardSummary> getDashboardSummary() =>
      remoteDataSource.getDashboardSummary();

  @override
  Future<bool> toggleAutoWatering({required bool enabled}) =>
      remoteDataSource.toggleAutoWatering(enabled: enabled);
}
