import 'package:flutter/material.dart';
import '../theme/app_colors.dart';

enum DeviceStatus { online, offline, error }

class SensorStatusBadge extends StatelessWidget {
  final DeviceStatus status;
  final String? label;

  const SensorStatusBadge({
    super.key,
    required this.status,
    this.label,
  });

  @override
  Widget build(BuildContext context) {
    final (color, text) = switch (status) {
      DeviceStatus.online => (AppColors.deviceOnline, label ?? 'Online'),
      DeviceStatus.offline => (AppColors.deviceOffline, label ?? 'Offline'),
      DeviceStatus.error => (AppColors.deviceError, label ?? 'Lỗi'),
    };

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
      decoration: BoxDecoration(
        color: color.withValues(alpha: 0.15),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: color.withValues(alpha: 0.4)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 8,
            height: 8,
            decoration: BoxDecoration(color: color, shape: BoxShape.circle),
          ),
          const SizedBox(width: 6),
          Text(
            text,
            style: Theme.of(context).textTheme.labelSmall?.copyWith(
              color: color,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }
}
