class ScheduleModel {
  final int? id;
  final String deviceId;
  final String startTime; // Format: "HH:MM:SS"
  final String endTime;   // Format: "HH:MM:SS"
  final String daysOfWeek; // Format: "MON,WED,FRI"
  final bool isEnabled;
  final int durationMinutes;

  const ScheduleModel({
    this.id,
    required this.deviceId,
    required this.startTime,
    required this.endTime,
    required this.daysOfWeek,
    required this.isEnabled,
    this.durationMinutes = 15,
  });

  factory ScheduleModel.fromJson(Map<String, dynamic> json) {
    return ScheduleModel(
      id: json['id'] is num ? (json['id'] as num).toInt() : null,
      deviceId: json['device_id'] ?? '',
      startTime: json['start_time']?.toString() ?? '00:00:00',
      endTime: json['end_time']?.toString() ?? '00:00:00',
      daysOfWeek: json['days_of_week'] ?? '',
      isEnabled: json['is_enabled'] == 1 || json['is_enabled'] == true,
      durationMinutes: json['duration_minutes'] is num
          ? (json['duration_minutes'] as num).toInt()
          : 15,
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'id': id,
      'device_id': deviceId,
      'start_time': startTime,
      'end_time': endTime,
      'days_of_week': daysOfWeek,
      'is_enabled': isEnabled,
      'duration_minutes': durationMinutes,
    };
  }
}
