import 'package:equatable/equatable.dart';

class SharedViewer extends Equatable {
  final int id;
  final String email;
  final bool isLocked;

  const SharedViewer({
    required this.id,
    required this.email,
    required this.isLocked,
  });

  @override
  List<Object?> get props => [id, email, isLocked];
}

/// Một thiết bị người dùng sở hữu cùng những người đang được chia sẻ.
class DeviceShare extends Equatable {
  final String deviceId;
  final String deviceName;
  final List<SharedViewer> viewers;

  const DeviceShare({
    required this.deviceId,
    required this.deviceName,
    required this.viewers,
  });

  bool isSharedWith(String email) => viewers.any(
        (viewer) => viewer.email.toLowerCase() == email.trim().toLowerCase(),
      );

  @override
  List<Object?> get props => [deviceId, deviceName, viewers];
}
