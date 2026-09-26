import '../../domain/entities/device.dart';

class DeviceModel extends Device {
  const DeviceModel({
    required super.id,
    required super.name,
    required super.type,
    required super.isOnline,
    super.isActive,
    super.location,
    super.lastSeen,
  });

  factory DeviceModel.fromJson(Map<String, dynamic> json) {
    return DeviceModel(
      id: json['id'] as int,
      name: json['name'] as String,
      type: _parseType(json['type'] as String?),
      isOnline: json['online'] as bool? ?? false,
      isActive: json['active'] as bool? ?? false,
      location: json['location'] as String?,
      lastSeen: json['lastSeen'] != null
          ? DateTime.tryParse(json['lastSeen'] as String)
          : null,
    );
  }

  static DeviceType _parseType(String? type) {
    return switch (type?.toUpperCase()) {
      'PUMP' => DeviceType.pump,
      'VALVE' => DeviceType.valve,
      'SENSOR' => DeviceType.sensor,
      'GATEWAY' => DeviceType.gateway,
      _ => DeviceType.sensor,
    };
  }
}
