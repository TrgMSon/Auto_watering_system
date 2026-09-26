import '../entities/dashboard_summary.dart';

abstract class DashboardRepository {
  Future<DashboardSummary> getDashboardSummary();
  Future<bool> toggleAutoWatering({required bool enabled});
}
