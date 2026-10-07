import 'package:flutter/material.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/sensor_status_badge.dart';
import '../../domain/entities/device.dart';

class DeviceCard extends StatelessWidget {
  final Device device;
  final ValueChanged<bool> onToggle;
  final VoidCallback onDelete;

  const DeviceCard({
    super.key,
    required this.device,
    required this.onToggle,
    required this.onDelete,
  });

  IconData get _deviceIcon => switch (device.type) {
    DeviceType.pump => Icons.water,
    DeviceType.valve => Icons.toggle_on_outlined,
    DeviceType.sensor => Icons.sensors,
    DeviceType.gateway => Icons.router,
  };

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: (device.isOnline ? AppColors.primary : AppColors.deviceOffline)
                    .withValues(alpha: 0.12),
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(
                _deviceIcon,
                color: device.isOnline ? AppColors.primary : AppColors.deviceOffline,
                size: 28,
              ),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(device.name, style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 4),
                  Row(
                    children: [
                      SensorStatusBadge(
                        status: device.isOnline ? DeviceStatus.online : DeviceStatus.offline,
                      ),
                      if (device.location != null) ...[
                        const SizedBox(width: 8),
                        Icon(Icons.location_on, size: 14, color: AppColors.textSecondary),
                        const SizedBox(width: 2),
                        Text(device.location!, style: Theme.of(context).textTheme.bodySmall),
                      ],
                    ],
                  ),
                ],
              ),
            ),
            if (device.type == DeviceType.pump || device.type == DeviceType.valve)
              Switch(
                value: device.isActive,
                onChanged: device.isOnline ? onToggle : null,
              ),
            PopupMenuButton<String>(
              onSelected: (value) {
                if (value == 'delete') onDelete();
              },
              itemBuilder: (context) => [
                const PopupMenuItem(
                  value: 'delete',
                  child: Text('X\u00f3a', style: TextStyle(color: AppColors.error)),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
