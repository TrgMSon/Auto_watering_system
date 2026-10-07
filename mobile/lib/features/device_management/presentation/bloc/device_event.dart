import 'package:equatable/equatable.dart';

sealed class DeviceEvent extends Equatable {
  const DeviceEvent();
  @override
  List<Object?> get props => [];
}

class DeviceLoadRequested extends DeviceEvent {
  const DeviceLoadRequested();
}

class DeviceAddRequested extends DeviceEvent {
  final String name;
  final String type;
  final String? location;
  const DeviceAddRequested({required this.name, required this.type, this.location});

  @override
  List<Object?> get props => [name, type, location];
}

class DeviceDeleteRequested extends DeviceEvent {
  final int deviceId;
  const DeviceDeleteRequested({required this.deviceId});

  @override
  List<Object?> get props => [deviceId];
}

class DeviceToggleRequested extends DeviceEvent {
  final int deviceId;
  final bool activate;
  const DeviceToggleRequested({required this.deviceId, required this.activate});

  @override
  List<Object?> get props => [deviceId, activate];
}
