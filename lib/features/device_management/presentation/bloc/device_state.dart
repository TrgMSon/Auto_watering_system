import 'package:equatable/equatable.dart';
import '../../domain/entities/device.dart';

sealed class DeviceState extends Equatable {
  const DeviceState();
  @override
  List<Object?> get props => [];
}

class DeviceInitial extends DeviceState {
  const DeviceInitial();
}

class DeviceLoading extends DeviceState {
  const DeviceLoading();
}

class DeviceLoaded extends DeviceState {
  final List<Device> devices;
  const DeviceLoaded({required this.devices});

  @override
  List<Object?> get props => [devices];
}

class DeviceError extends DeviceState {
  final String message;
  const DeviceError({required this.message});

  @override
  List<Object?> get props => [message];
}
