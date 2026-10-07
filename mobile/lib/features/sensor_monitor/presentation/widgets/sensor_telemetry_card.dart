import 'package:flutter/material.dart';
import '../../../../core/theme/app_colors.dart';

class SensorTelemetryCard extends StatelessWidget {
  final String title;
  final double currentValue;
  final String unit;
  final IconData icon;
  final Color color;
  final bool isWarning;

  const SensorTelemetryCard({
    super.key,
    required this.title,
    required this.currentValue,
    required this.unit,
    required this.icon,
    required this.color,
    this.isWarning = false,
  });

  @override
  Widget build(BuildContext context) {
    final accentColor = isWarning ? AppColors.warning : color;

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Row(
          children: [
            Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: accentColor.withValues(alpha: 0.12),
                borderRadius: BorderRadius.circular(10),
              ),
              child: Icon(icon, color: accentColor, size: 28),
            ),
            const SizedBox(width: 16),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    title,
                    style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                      color: AppColors.textSecondary,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Row(
                    crossAxisAlignment: CrossAxisAlignment.baseline,
                    textBaseline: TextBaseline.alphabetic,
                    children: [
                      Text(
                        currentValue.toStringAsFixed(1),
                        style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                          fontWeight: FontWeight.bold,
                          color: accentColor,
                        ),
                      ),
                      const SizedBox(width: 4),
                      Text(unit, style: Theme.of(context).textTheme.bodySmall),
                    ],
                  ),
                ],
              ),
            ),
            if (isWarning)
              const Icon(Icons.warning_amber_rounded, color: AppColors.warning, size: 24),
          ],
        ),
      ),
    );
  }
}
