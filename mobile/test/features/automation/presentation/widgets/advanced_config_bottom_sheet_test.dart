import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/automation/presentation/widgets/advanced_config_bottom_sheet.dart';

void main() {
  Future<void> pumpSheet(WidgetTester tester, int maxWateringDuration) async {
    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: AdvancedConfigBottomSheet(
          currentConfig: DeviceConfigModel(
            deviceId: '38:3E:51:6F:C7:A8',
            autoMode: true,
            soilMoistureMin: 40,
            soilMoistureMax: 80,
            airHumidityMin: 50,
            airHumidityMax: 80,
            temperatureMin: 20,
            temperatureMax: 35,
            maxWateringDuration: maxWateringDuration,
          ),
        ),
      ),
    ));
  }

  testWidgets('hiển thị thời gian bơm theo phút', (tester) async {
    await pumpSheet(tester, 2);

    expect(find.text('Thời gian bơm tối đa: 2 phút'), findsOneWidget);
  });

  testWidgets('dữ liệu cũ lưu theo giây không làm hỏng màn hình', (tester) async {
    await pumpSheet(tester, 120);

    expect(tester.takeException(), isNull);
    expect(find.text('Thời gian bơm tối đa: 10 phút'), findsOneWidget);
  });

  testWidgets('nút Mặc định đưa về 2 phút', (tester) async {
    await pumpSheet(tester, 5);

    await tester.tap(find.text('Mặc định'));
    await tester.pump();

    expect(find.text('Thời gian bơm tối đa: 2 phút'), findsOneWidget);
  });
}
