import 'package:flutter/material.dart';
import '../theme/app_colors.dart';

class EmptyDeviceWidget extends StatelessWidget {
  final String title;
  final String subtitle;
  final VoidCallback? onReload;

  const EmptyDeviceWidget({
    super.key,
    this.title = 'Bạn chưa có thiết bị nào.',
    this.subtitle = 'Liên hệ quản trị viên để được gán thiết bị',
    this.onReload,
  });

  @override
  Widget build(BuildContext context) {
    return Center(
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(
              Icons.devices_other,
              size: 64,
              color: AppColors.deviceOffline,
            ),
            const SizedBox(height: 16),
            Text(
              title,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
            ),
            const SizedBox(height: 8),
            Text(
              subtitle,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: AppColors.textSecondary,
                  ),
            ),
            if (onReload != null) ...[
              const SizedBox(height: 24),
              ElevatedButton.icon(
                onPressed: onReload,
                icon: const Icon(Icons.refresh),
                label: const Text('Tải lại'),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

