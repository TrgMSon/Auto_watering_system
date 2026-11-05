import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:intl/intl.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../domain/entities/sensor_reading.dart';
import '../../domain/entities/statistics_point.dart';
import '../bloc/statistics_bloc.dart';
import 'telemetry_line_chart.dart';

class StatisticsSection extends StatelessWidget {
  final String deviceId;

  const StatisticsSection({super.key, required this.deviceId});

  void _load(BuildContext context, StatisticsRange range) {
    context.read<StatisticsBloc>().add(
          StatisticsRequested(deviceId: deviceId, range: range),
        );
  }

  @override
  Widget build(BuildContext context) {
    return BlocBuilder<StatisticsBloc, StatisticsState>(
      builder: (context, state) {
        return Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              'Trung bình theo giờ',
              style: Theme.of(context).textTheme.titleMedium?.copyWith(
                    fontWeight: FontWeight.w600,
                  ),
            ),
            const SizedBox(height: 12),
            SegmentedButton<StatisticsRange>(
              showSelectedIcon: false,
              segments: [
                for (final range in StatisticsRange.values)
                  ButtonSegment(value: range, label: Text(range.label)),
              ],
              selected: {state.range},
              onSelectionChanged: (selection) => _load(context, selection.first),
            ),
            const SizedBox(height: 12),
            _buildContent(context, state),
          ],
        );
      },
    );
  }

  Widget _buildContent(BuildContext context, StatisticsState state) {
    if (state.isLoading) {
      return const SizedBox(
        height: 200,
        child: AppLoadingIndicator(message: 'Đang tải thống kê...'),
      );
    }
    if (state.errorMessage != null) {
      return SizedBox(
        height: 240,
        child: AppErrorWidget(
          message: state.errorMessage!,
          onRetry: () => _load(context, state.range),
        ),
      );
    }
    if (state.points.isEmpty) {
      return const SizedBox(
        height: 120,
        child: Center(child: Text('Chưa có dữ liệu thống kê trong khoảng này')),
      );
    }
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        _buildChart(state.points, 'Độ ẩm đất', '%', AppColors.moisture, (p) => p.soilMoisture),
        _buildChart(state.points, 'Nhiệt độ', '°C', AppColors.temperature, (p) => p.temperature),
        _buildChart(state.points, 'Độ ẩm không khí', '%', AppColors.humidity, (p) => p.humidity),
        _buildChart(state.points, 'Mực nước', 'cm', AppColors.waterLevel, (p) => p.waterLevel),
      ],
    );
  }

  Widget _buildChart(
    List<StatisticsPoint> points,
    String title,
    String unit,
    Color color,
    double? Function(StatisticsPoint) value,
  ) {
    final readings = [
      for (final point in points)
        if (value(point) case final double pointValue)
          SensorReading(
            id: 0,
            soilMoisture: pointValue,
            temperature: 0,
            humidity: 0,
            lightIntensity: 0,
            timestamp: point.time,
          ),
    ];
    if (readings.isEmpty) return const SizedBox.shrink();

    final format = DateFormat('dd/MM HH:mm');
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Card(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            TelemetryLineChart(
              readings: readings,
              title: title,
              valueExtractor: (reading) => reading.soilMoisture,
              color: color,
              unit: unit,
            ),
            Padding(
              padding: const EdgeInsets.fromLTRB(16, 0, 16, 12),
              child: Text(
                '${format.format(readings.first.timestamp)} – '
                '${format.format(readings.last.timestamp)}',
                textAlign: TextAlign.center,
                style: const TextStyle(fontSize: 12, color: AppColors.textSecondary),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
