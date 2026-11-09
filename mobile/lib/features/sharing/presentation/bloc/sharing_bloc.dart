import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/repositories/sharing_repository.dart';
import 'sharing_event.dart';
import 'sharing_state.dart';

class SharingBloc extends Bloc<SharingEvent, SharingState> {
  final SharingRepository repository;

  SharingBloc({required this.repository}) : super(const SharingInitial()) {
    on<SharingLoadRequested>(_onLoadRequested);
    on<SharingViewerAdded>(_onViewerAdded);
    on<SharingViewerRemoved>(_onViewerRemoved);
    on<SharingViewerLockChanged>(_onViewerLockChanged);
  }

  Future<void> _onLoadRequested(
    SharingLoadRequested event,
    Emitter<SharingState> emit,
  ) async {
    emit(const SharingLoading());
    try {
      emit(SharingLoaded(shares: await repository.getOwnedDeviceShares()));
    } catch (e) {
      emit(SharingError(message: e.toString()));
    }
  }

  Future<void> _onViewerAdded(
    SharingViewerAdded event,
    Emitter<SharingState> emit,
  ) async {
    final current = state;
    if (current is! SharingLoaded) return;
    emit(current.savingCopy());

    var sharedCount = 0;
    String? failure;
    for (final deviceId in event.deviceIds) {
      try {
        await repository.addViewer(deviceId, event.email);
        sharedCount++;
      } catch (e) {
        failure = e.toString();
      }
    }

    await _reload(
      emit,
      current,
      errorMessage: failure == null
          ? null
          : 'Đã chia sẻ $sharedCount/${event.deviceIds.length} thiết bị. $failure',
      shareCompleted: failure == null,
    );
  }

  Future<void> _onViewerRemoved(
    SharingViewerRemoved event,
    Emitter<SharingState> emit,
  ) {
    return _runChange(
      emit,
      () => repository.removeViewer(event.deviceId, event.viewerId),
    );
  }

  Future<void> _onViewerLockChanged(
    SharingViewerLockChanged event,
    Emitter<SharingState> emit,
  ) {
    return _runChange(
      emit,
      () => repository.setViewerLocked(
        event.deviceId,
        event.viewerId,
        locked: event.locked,
      ),
    );
  }

  Future<void> _runChange(
    Emitter<SharingState> emit,
    Future<void> Function() change,
  ) async {
    final current = state;
    if (current is! SharingLoaded) return;
    emit(current.savingCopy());

    String? failure;
    try {
      await change();
    } catch (e) {
      failure = e.toString();
    }
    await _reload(emit, current, errorMessage: failure);
  }

  Future<void> _reload(
    Emitter<SharingState> emit,
    SharingLoaded previous, {
    String? errorMessage,
    bool shareCompleted = false,
  }) async {
    try {
      emit(SharingLoaded(
        shares: await repository.getOwnedDeviceShares(),
        errorMessage: errorMessage,
        shareCompleted: shareCompleted,
      ));
    } catch (e) {
      emit(SharingLoaded(
        shares: previous.shares,
        errorMessage: errorMessage ?? e.toString(),
        shareCompleted: shareCompleted,
      ));
    }
  }
}
