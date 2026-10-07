import '../repositories/dashboard_repository.dart';

class ToggleAutoWateringUseCase {
  final DashboardRepository repository;
  const ToggleAutoWateringUseCase(this.repository);

  Future<bool> call({required bool enabled}) =>
      repository.toggleAutoWatering(enabled: enabled);
}
