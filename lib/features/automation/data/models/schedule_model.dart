class ScheduleModel {
  final int? id;
  final String deviceId;
  final String startTime; // Format: "HH:MM:SS"
  final String endTime;   // Format: "HH:MM:SS"
  final String daysOfWeek; // Format: "MON,WED,FRI"
  final bool isEnabled;

  const ScheduleModel({
    this.id,
    required this.deviceId,
    required this.startTime,
    required this.endTime,
    required this.daysOfWeek,
    required this.isEnabled,
  });

  factory ScheduleModel.fromJson(Map<String, dynamic> json) {
    return ScheduleModel(
      id: json['id'],
      deviceId: json['device_id'] ?? '',
      startTime: json['start_time'] ?? '00:00:00',
      endTime: json['end_time'] ?? '00:00:00',
      daysOfWeek: json['days_of_week'] ?? '',
      isEnabled: json['is_enabled'] == 1 || json['is_enabled'] == true,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'device_id': deviceId,
      'start_time': startTime,
      'end_time': endTime,
      'days_of_week': daysOfWeek,
      'is_enabled': isEnabled ? 1 : 0,
    };
  }
}
