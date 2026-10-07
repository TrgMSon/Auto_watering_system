import 'package:equatable/equatable.dart';

class DeviceModel extends Equatable {
  final String id;
  final String deviceName;
  final String status;
  final String permission;
  final String? blePassKey;

  const DeviceModel({
    required this.id,
    required this.deviceName,
    required this.status,
    required this.permission,
    this.blePassKey,
  });

  factory DeviceModel.fromJson(Map<String, dynamic> json) {
    return DeviceModel(
      id: json['id'] ?? '',
      deviceName: json['device_name'] ?? json['deviceName'] ?? '',
      status: json['status'] ?? 'OFFLINE',
      permission: json['permission'] ?? 'VIEWER',
      blePassKey: json['ble_pass_key'] ?? json['blePassKey'],
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'device_name': deviceName,
      'status': status,
      'permission': permission,
      'ble_pass_key': blePassKey,
    };
  }

  @override
  List<Object?> get props => [id, deviceName, status, permission, blePassKey];
}
