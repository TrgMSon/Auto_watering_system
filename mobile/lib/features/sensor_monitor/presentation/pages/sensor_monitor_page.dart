import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../domain/entities/sensor_reading.dart';
import '../bloc/sensor_bloc.dart';
import '../bloc/sensor_event.dart';
import '../bloc/sensor_state.dart';
import '../bloc/statistics_bloc.dart';
import '../widgets/sensor_telemetry_card.dart';
import '../widgets/statistics_section.dart';

class SensorMonitorPage extends StatefulWidget {
  final String deviceId;

  const SensorMonitorPage({super.key, this.deviceId = ''});

  @override
  State<SensorMonitorPage> createState() => _SensorMonitorPageState();
}

class _SensorMonitorPageState extends State<SensorMonitorPage> {
  @override
  void initState() {
    super.initState();
    _fetchData();
  }

  @override
  void didUpdateWidget(covariant SensorMonitorPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.deviceId != oldWidget.deviceId) {
      if (widget.deviceId.isNotEmpty) {
        _fetchData();
      } else {
        context.read<SensorBloc>().add(const SensorRealtimeStopped());
      }
    }
  }

  void _fetchData() {
    if (widget.deviceId.isEmpty) return;
    final now = DateTime.now();
    context.read<SensorBloc>()
      ..add(SensorHistoryRequested(
        deviceId: widget.deviceId,
        from: now.subtract(const Duration(hours: 24)),
        to: now,
      ))
      ..add(SensorRealtimeStarted(deviceId: widget.deviceId));
    final statisticsBloc = context.read<StatisticsBloc>();
    statisticsBloc.add(StatisticsRequested(
      deviceId: widget.deviceId,
      range: statisticsBloc.state.range,
    ));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Thống kê cảm biến')),
      body: widget.deviceId.isEmpty
          ? const Center(child: Text('Chưa có thiết bị'))
          : BlocBuilder<SensorBloc, SensorState>(
              builder: (context, state) {
                if (state is SensorInitial || state is SensorLoading) {
                  return const AppLoadingIndicator(
                    message: 'Đang tải dữ liệu cảm biến...',
                  );
                }
                if (state is SensorError) {
                  return AppErrorWidget(
                    message: state.message,
                    onRetry: _fetchData,
                  );
                }
                if (state is SensorLoaded) {
                  return _buildLoadedBody(context, state);
                }
                return const SizedBox.shrink();
              },
            ),
    );
  }

  Widget _buildLoadedBody(BuildContext context, SensorLoaded state) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        if (state.errorMessage != null) ...[
          Padding(
            padding: const EdgeInsets.only(bottom: 12),
            child: Text(
              state.errorMessage!,
              style: const TextStyle(color: Colors.red, fontSize: 13),
            ),
          ),
        ],
        if (state.latestReading != null) ...[
          _buildLatestCards(context, state.latestReading!),
          const SizedBox(height: 24),
        ],
        StatisticsSection(deviceId: widget.deviceId),
      ],
    );
  }

  Widget _buildLatestCards(BuildContext context, SensorReading reading) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(
          'Dữ liệu hiện tại',
          style: Theme.of(context).textTheme.titleMedium?.copyWith(
                fontWeight: FontWeight.w600,
              ),
        ),
        const SizedBox(height: 12),
        SensorTelemetryCard(
          title: 'Độ ẩm đất',
          currentValue: reading.soilMoisture,
          unit: '%',
          icon: Icons.water_drop,
          color: AppColors.moisture,
          isWarning: reading.soilMoisture < 30,
        ),
        const SizedBox(height: 8),
        SensorTelemetryCard(
          title: 'Nhiệt độ',
          currentValue: reading.temperature,
          unit: '°C',
          icon: Icons.thermostat,
          color: AppColors.temperature,
          isWarning: reading.temperature > 40,
        ),
        const SizedBox(height: 8),
        SensorTelemetryCard(
          title: 'Độ ẩm không khí',
          currentValue: reading.humidity,
          unit: '%',
          icon: Icons.cloud,
          color: AppColors.humidity,
        ),
      ],
    );
  }
}
