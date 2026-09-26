import 'package:equatable/equatable.dart';

enum DeviceType { pump, valve, sensor, gateway }

class Device extends Equatable {
  final int id;
  final String name;
  final DeviceType type;
  final bool isOnline;
  final bool isActive;
  final String? location;
  final DateTime? lastSeen;

  const Device({
    required this.id,
    required this.name,
    required this.type,
    required this.isOnline,
    this.isActive = false,
    this.location,
    this.lastSeen,
  });

  @override
  List<Object?> get props => [id, name, type, isOnline, isActive, location, lastSeen];
}
