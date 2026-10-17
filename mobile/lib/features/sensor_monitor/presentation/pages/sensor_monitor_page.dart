import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../bloc/sensor_bloc.dart';
import '../bloc/sensor_event.dart';
import '../bloc/sensor_state.dart';
import '../widgets/sensor_telemetry_card.dart';
import '../widgets/telemetry_line_chart.dart';

class SensorMonitorPage extends StatefulWidget {
  const SensorMonitorPage({super.key});

  @override
  State<SensorMonitorPage> createState() => _SensorMonitorPageState();
}

class _SensorMonitorPageState extends State<SensorMonitorPage> {
  @override
  void initState() {
    super.initState();
    final now = DateTime.now();
    context.read<SensorBloc>()
      ..add(SensorHistoryRequested(
        from: now.subtract(const Duration(hours: 24)),
        to: now,
      ))
      ..add(const SensorRealtimeStarted());
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Th\u1ed1ng k\u00ea c\u1ea3m bi\u1ebfn')),
      body: BlocBuilder<SensorBloc, SensorState>(
        builder: (context, state) {
          if (state is SensorInitial || state is SensorLoading) {
            return const AppLoadingIndicator(message: '\u0110ang t\u1ea3i d\u1eef li\u1ec7u c\u1ea3m bi\u1ebfn...');
          }
          if (state is SensorError) {
            return AppErrorWidget(
              message: state.message,
              onRetry: () {
                final now = DateTime.now();
                context.read<SensorBloc>().add(SensorHistoryRequested(
                  from: now.subtract(const Duration(hours: 24)),
                  to: now,
                ));
              },
            );
          }
          if (state is SensorLoaded) {
            final latestReading = state.latestReading;
            final chartData = state.realtimeBuffer.isNotEmpty
                ? state.realtimeBuffer
                : state.historyReadings;

            return ListView(
              padding: const EdgeInsets.all(16),
              children: [
                if (latestReading != null) ...[
                  Text(
                    'D\u1eef li\u1ec7u hi\u1ec7n t\u1ea1i',
                    style: Theme.of(context).textTheme.titleMedium?.copyWith(
                      fontWeight: FontWeight.w600,
                    ),
                  ),
                  const SizedBox(height: 12),
                  SensorTelemetryCard(
                    title: '\u0110\u1ed9 \u1ea9m \u0111\u1ea5t',
                    currentValue: latestReading.soilMoisture,
                    unit: '%',
                    icon: Icons.water_drop,
                    color: AppColors.moisture,
                    isWarning: latestReading.soilMoisture < 30,
                  ),
                  const SizedBox(height: 8),
                  SensorTelemetryCard(
                    title: 'Nhi\u1ec7t \u0111\u1ed9',
                    currentValue: latestReading.temperature,
                    unit: '\u00b0C',
                    icon: Icons.thermostat,
                    color: AppColors.temperature,
                    isWarning: latestReading.temperature > 40,
                  ),
                  const SizedBox(height: 8),
                  SensorTelemetryCard(
                    title: '\u0110\u1ed9 \u1ea9m kh\u00f4ng kh\u00ed',
                    currentValue: latestReading.humidity,
                    unit: '%',
                    icon: Icons.cloud,
                    color: AppColors.humidity,
                  ),
                  const SizedBox(height: 8),
                  SensorTelemetryCard(
                    title: 'C\u01b0\u1eddng \u0111\u1ed9 \u00e1nh s\u00e1ng',
                    currentValue: latestReading.lightIntensity,
                    unit: 'lux',
                    icon: Icons.wb_sunny,
                    color: AppColors.light,
                  ),
                  const SizedBox(height: 24),
                ],
                Text(
                  'Bi\u1ec3u \u0111\u1ed3 24 gi\u1edd qua',
                  style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
                ),
                const SizedBox(height: 12),
                Card(
                  child: TelemetryLineChart(
                    readings: chartData,
                    title: '\u0110\u1ed9 \u1ea9m \u0111\u1ea5t',
                    valueExtractor: (r) => r.soilMoisture,
                    color: AppColors.moisture,
                    unit: '%',
                  ),
                ),
                const SizedBox(height: 12),
                Card(
                  child: TelemetryLineChart(
                    readings: chartData,
                    title: 'Nhi\u1ec7t \u0111\u1ed9',
                    valueExtractor: (r) => r.temperature,
                    color: AppColors.temperature,
                    unit: '\u00b0C',
                  ),
                ),
                const SizedBox(height: 12),
                Card(
                  child: TelemetryLineChart(
                    readings: chartData,
                    title: '\u0110\u1ed9 \u1ea9m kh\u00f4ng kh\u00ed',
                    valueExtractor: (r) => r.humidity,
                    color: AppColors.humidity,
                    unit: '%',
                  ),
                ),
              ],
            );
          }
          return const SizedBox.shrink();
        },
      ),
    );
  }
}
