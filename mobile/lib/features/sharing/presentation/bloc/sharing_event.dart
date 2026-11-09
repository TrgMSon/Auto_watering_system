import 'package:equatable/equatable.dart';

sealed class SharingEvent extends Equatable {
  const SharingEvent();

  @override
  List<Object?> get props => [];
}

class SharingLoadRequested extends SharingEvent {
  const SharingLoadRequested();
}

class SharingViewerAdded extends SharingEvent {
  final String email;
  final List<String> deviceIds;

  const SharingViewerAdded({required this.email, required this.deviceIds});

  @override
  List<Object?> get props => [email, deviceIds];
}

class SharingViewerRemoved extends SharingEvent {
  final String deviceId;
  final int viewerId;

  const SharingViewerRemoved({required this.deviceId, required this.viewerId});

  @override
  List<Object?> get props => [deviceId, viewerId];
}

class SharingViewerLockChanged extends SharingEvent {
  final String deviceId;
  final int viewerId;
  final bool locked;

  const SharingViewerLockChanged({
    required this.deviceId,
    required this.viewerId,
    required this.locked,
  });

  @override
  List<Object?> get props => [deviceId, viewerId, locked];
}
