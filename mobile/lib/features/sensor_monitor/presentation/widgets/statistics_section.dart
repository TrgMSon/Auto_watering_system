import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../domain/entities/statistics_point.dart';
import '../bloc/statistics_bloc.dart';
import 'statistics_line_chart.dart';

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
    final charts = [
      ('Độ ẩm đất', '%', AppColors.moisture, (StatisticsPoint p) => p.soilMoisture),
      ('Nhiệt độ', '°C', AppColors.temperature, (StatisticsPoint p) => p.temperature),
      ('Độ ẩm không khí', '%', AppColors.humidity, (StatisticsPoint p) => p.humidity),
      ('Mực nước', '%', AppColors.waterLevel, (StatisticsPoint p) => p.waterLevel),
    ];
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        for (final (title, unit, color, valueOf) in charts)
          StatisticsLineChart(
            points: state.points,
            range: state.range,
            title: title,
            unit: unit,
            color: color,
            valueOf: valueOf,
          ),
      ],
    );
  }
}
