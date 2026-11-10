import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:intl/date_symbol_data_local.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/statistics_point.dart';
import 'package:watering_app/features/sensor_monitor/presentation/widgets/statistics_line_chart.dart';

void main() {
  setUpAll(() => initializeDateFormatting());

  Future<void> pump(WidgetTester tester, List<StatisticsPoint> points) {
    return tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: SingleChildScrollView(
          child: StatisticsLineChart(
            points: points,
            range: StatisticsRange.day,
            title: 'Độ ẩm đất',
            unit: '%',
            color: Colors.orange,
            valueOf: (p) => p.soilMoisture,
          ),
        ),
      ),
    ));
  }

  testWidgets('shows a visible dot and a note when only one hour has data', (tester) async {
    await pump(tester, [StatisticsPoint(time: DateTime(2026, 11, 10, 9), soilMoisture: 23)]);

    final data = tester.widget<LineChart>(find.byType(LineChart)).data;
    expect(data.lineBarsData.single.dotData.show, isTrue);
    expect(data.minY, lessThan(23));
    expect(data.maxY, greaterThan(23));
    expect(data.minX, lessThan(data.maxX));
    expect(find.text('Mới có số liệu của 1 giờ'), findsOneWidget);
  });

  testWidgets('places points by time and shows min, average and max', (tester) async {
    await pump(tester, [
      StatisticsPoint(time: DateTime(2026, 11, 10, 9), soilMoisture: 20),
      StatisticsPoint(time: DateTime(2026, 11, 10, 8), soilMoisture: 30),
      StatisticsPoint(time: DateTime(2026, 11, 10, 12)),
      StatisticsPoint(time: DateTime(2026, 11, 10, 13), soilMoisture: 40),
    ]);

    final spots = tester.widget<LineChart>(find.byType(LineChart)).data.lineBarsData.single.spots;
    expect(spots.map((s) => (s.x, s.y)), [(0.0, 30.0), (1.0, 20.0), (5.0, 40.0)]);
    expect(find.text('20.0 %'), findsOneWidget);
    expect(find.text('30.0 %'), findsOneWidget);
    expect(find.text('40.0 %'), findsNWidgets(2));
  });

  testWidgets('renders nothing when the series has no values', (tester) async {
    await pump(tester, [StatisticsPoint(time: DateTime(2026, 11, 10, 9), temperature: 20)]);

    expect(find.byType(LineChart), findsNothing);
  });
}
