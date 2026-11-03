import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/sensor_reading.dart';
import 'package:watering_app/features/sensor_monitor/presentation/widgets/telemetry_line_chart.dart';

void main() {
  final testReading = SensorReading(
    id: 1,
    soilMoisture: 45.0,
    temperature: 25.0,
    humidity: 60.0,
    lightIntensity: 0,
    timestamp: DateTime(2026, 11, 2, 10, 0),
  );

  Widget createWidget(List<SensorReading> readings) {
    return MaterialApp(
      home: Scaffold(
        body: TelemetryLineChart(
          readings: readings,
          title: 'Độ ẩm đất',
          unit: '%',
          color: Colors.blue,
          valueExtractor: (r) => r.soilMoisture,
        ),
      ),
    );
  }

  group('TelemetryLineChart', () {
    testWidgets('renders empty placeholder when readings list is empty',
        (tester) async {
      await tester.pumpWidget(createWidget([]));

      expect(find.text('Chưa có dữ liệu'), findsOneWidget);
    });

    testWidgets('renders title with resolved variables like "Độ ẩm đất (%)"',
        (tester) async {
      await tester.pumpWidget(createWidget([testReading]));

      expect(find.text('Độ ẩm đất (%)'), findsOneWidget);
      expect(find.text(r'$title ($unit)'), findsNothing);
    });
  });
}

