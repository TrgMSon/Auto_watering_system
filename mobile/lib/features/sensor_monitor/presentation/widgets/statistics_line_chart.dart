import 'dart:math' as math;

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import '../../../../core/theme/app_colors.dart';
import '../../domain/entities/statistics_point.dart';

/// Biểu đồ trung bình theo giờ, trục ngang theo thời gian thật để thấy khoảng trống dữ liệu.
class StatisticsLineChart extends StatelessWidget {
  final List<StatisticsPoint> points;
  final StatisticsRange range;
  final String title;
  final String unit;
  final Color color;
  final double? Function(StatisticsPoint) valueOf;

  const StatisticsLineChart({
    super.key,
    required this.points,
    required this.range,
    required this.title,
    required this.unit,
    required this.color,
    required this.valueOf,
  });

  static const _hour = 3600000.0;

  @override
  Widget build(BuildContext context) {
    final samples = [
      for (final point in points)
        if (valueOf(point) case final double value) (time: point.time, value: value),
    ]..sort((a, b) => a.time.compareTo(b.time));
    if (samples.isEmpty) return const SizedBox.shrink();

    final start = samples.first.time;
    double xOf(DateTime time) => time.difference(start).inMilliseconds / _hour;
    final spots = [for (final s in samples) FlSpot(xOf(s.time), s.value)];

    final values = samples.map((s) => s.value);
    final minValue = values.reduce(math.min);
    final maxValue = values.reduce(math.max);
    final average = values.reduce((a, b) => a + b) / samples.length;
    final latest = samples.last.value;

    // Thêm lề để đường không dính mép, kể cả khi chỉ có một giá trị.
    final padding = maxValue == minValue ? math.max(1.0, maxValue.abs() * 0.1) : (maxValue - minValue) * 0.15;
    var minY = minValue - padding;
    final maxY = maxValue + padding;
    if (minValue >= 0 && minY < 0) minY = 0;

    // Một điểm thì cho trục ngang rộng 1 giờ về mỗi phía để điểm nằm giữa.
    final lastX = spots.last.x;
    final minX = lastX == 0 ? -1.0 : 0.0;
    final maxX = lastX == 0 ? 1.0 : lastX;
    final xInterval = math.max((maxX - minX) / 3, 1.0);

    final axisFormat = DateFormat(range == StatisticsRange.day ? 'HH:mm' : 'dd/MM');
    final tooltipFormat = DateFormat(range == StatisticsRange.day ? 'HH:mm' : 'dd/MM HH:mm');
    DateTime timeOf(double x) => start.add(Duration(milliseconds: (x * _hour).round()));
    String format(double value) => value.toStringAsFixed(1);

    final textTheme = Theme.of(context).textTheme;
    const axisStyle = TextStyle(fontSize: 10, color: AppColors.textSecondary);

    return Card(
      margin: const EdgeInsets.only(bottom: 12),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 12, 16, 8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Row(
              children: [
                Container(
                  width: 10,
                  height: 10,
                  decoration: BoxDecoration(color: color, shape: BoxShape.circle),
                ),
                const SizedBox(width: 8),
                Expanded(
                  child: Text(
                    '$title ($unit)',
                    style: textTheme.titleSmall?.copyWith(fontWeight: FontWeight.w600),
                  ),
                ),
                Text(
                  '${format(latest)} $unit',
                  style: textTheme.titleSmall?.copyWith(color: color, fontWeight: FontWeight.w700),
                ),
              ],
            ),
            const SizedBox(height: 4),
            Row(
              children: [
                _Stat(label: 'Thấp nhất', value: '${format(minValue)} $unit'),
                _Stat(label: 'Trung bình', value: '${format(average)} $unit'),
                _Stat(label: 'Cao nhất', value: '${format(maxValue)} $unit'),
              ],
            ),
            const SizedBox(height: 12),
            SizedBox(
              height: 180,
              child: LineChart(
                LineChartData(
                  minX: minX,
                  maxX: maxX,
                  minY: minY,
                  maxY: maxY,
                  clipData: const FlClipData.all(),
                  gridData: FlGridData(
                    drawVerticalLine: false,
                    horizontalInterval: (maxY - minY) / 4,
                    getDrawingHorizontalLine: (_) => FlLine(
                      color: Colors.grey.withValues(alpha: 0.15),
                      strokeWidth: 1,
                    ),
                  ),
                  borderData: FlBorderData(show: false),
                  titlesData: FlTitlesData(
                    topTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    rightTitles: const AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    leftTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 36,
                        interval: (maxY - minY) / 4,
                        getTitlesWidget: (value, meta) {
                          if (value == meta.min || value == meta.max) return const SizedBox.shrink();
                          return Text(value.toStringAsFixed(0), style: axisStyle);
                        },
                      ),
                    ),
                    bottomTitles: AxisTitles(
                      sideTitles: SideTitles(
                        showTitles: true,
                        reservedSize: 24,
                        interval: xInterval,
                        getTitlesWidget: (value, meta) {
                          if (lastX == 0 && value != 0) return const SizedBox.shrink();
                          return SideTitleWidget(
                            meta: meta,
                            child: Text(axisFormat.format(timeOf(value)), style: axisStyle),
                          );
                        },
                      ),
                    ),
                  ),
                  lineTouchData: LineTouchData(
                    touchTooltipData: LineTouchTooltipData(
                      getTooltipItems: (touched) => [
                        for (final spot in touched)
                          LineTooltipItem(
                            '${tooltipFormat.format(timeOf(spot.x))}\n',
                            const TextStyle(color: Colors.white70, fontSize: 11),
                            children: [
                              TextSpan(
                                text: '${format(spot.y)} $unit',
                                style: const TextStyle(
                                  color: Colors.white,
                                  fontWeight: FontWeight.bold,
                                  fontSize: 12,
                                ),
                              ),
                            ],
                          ),
                      ],
                    ),
                  ),
                  lineBarsData: [
                    LineChartBarData(
                      spots: spots,
                      isCurved: spots.length > 2,
                      preventCurveOverShooting: true,
                      color: color,
                      barWidth: 2.5,
                      isStrokeCapRound: true,
                      // Ít điểm thì hiện chấm, nếu không một giờ dữ liệu sẽ không nhìn thấy.
                      dotData: FlDotData(
                        show: spots.length <= 48,
                        getDotPainter: (spot, percent, bar, index) => FlDotCirclePainter(
                          radius: spots.length == 1 ? 5 : 3,
                          color: color,
                          strokeWidth: 1.5,
                          strokeColor: Colors.white,
                        ),
                      ),
                      belowBarData: BarAreaData(
                        show: true,
                        color: color.withValues(alpha: 0.12),
                      ),
                    ),
                  ],
                ),
              ),
            ),
            if (samples.length == 1)
              const Padding(
                padding: EdgeInsets.only(top: 4),
                child: Text(
                  'Mới có số liệu của 1 giờ',
                  textAlign: TextAlign.center,
                  style: TextStyle(fontSize: 12, color: AppColors.textSecondary),
                ),
              ),
          ],
        ),
      ),
    );
  }
}

class _Stat extends StatelessWidget {
  final String label;
  final String value;

  const _Stat({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Expanded(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: const TextStyle(fontSize: 11, color: AppColors.textSecondary)),
          Text(value, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w600)),
        ],
      ),
    );
  }
}
