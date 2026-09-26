import 'package:equatable/equatable.dart';
import '../../domain/entities/dashboard_summary.dart';

sealed class DashboardState extends Equatable {
  const DashboardState();

  @override
  List<Object?> get props => [];
}

class DashboardInitial extends DashboardState {
  const DashboardInitial();
}

class DashboardLoading extends DashboardState {
  const DashboardLoading();
}

class DashboardLoaded extends DashboardState {
  final DashboardSummary summary;
  final bool isTogglingWatering;

  const DashboardLoaded({
    required this.summary,
    this.isTogglingWatering = false,
  });

  DashboardLoaded copyWith({
    DashboardSummary? summary,
    bool? isTogglingWatering,
  }) {
    return DashboardLoaded(
      summary: summary ?? this.summary,
      isTogglingWatering: isTogglingWatering ?? this.isTogglingWatering,
    );
  }

  @override
  List<Object?> get props => [summary, isTogglingWatering];
}

class DashboardError extends DashboardState {
  final String message;
  const DashboardError({required this.message});

  @override
  List<Object?> get props => [message];
}
