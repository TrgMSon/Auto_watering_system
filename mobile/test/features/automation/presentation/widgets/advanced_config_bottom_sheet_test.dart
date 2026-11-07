import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_bloc.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_event.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_state.dart';
import 'package:watering_app/features/automation/presentation/widgets/advanced_config_bottom_sheet.dart';

class MockAutomationBloc extends MockBloc<AutomationEvent, AutomationState>
    implements AutomationBloc {}

DeviceConfigModel _config({int waterLevelMin = 20}) => DeviceConfigModel(
      deviceId: '38:3E:51:6F:C7:A8',
      autoMode: false,
      soilMoistureMin: 40,
      soilMoistureMax: 80,
      airHumidityMin: 50,
      airHumidityMax: 80,
      temperatureMin: 20,
      temperatureMax: 35,
      maxWateringDuration: 2,
      waterLevelMin: waterLevelMin,
    );

void main() {
  late MockAutomationBloc bloc;

  setUpAll(() {
    registerFallbackValue(AutomationConfigUpdated(_config()));
  });

  setUp(() {
    bloc = MockAutomationBloc();
    when(() => bloc.state).thenReturn(const AutomationInitial());
  });

  Future<void> pumpSheet(WidgetTester tester, DeviceConfigModel config) async {
    await tester.pumpWidget(MaterialApp(
      home: Scaffold(
        body: BlocProvider<AutomationBloc>.value(
          value: bloc,
          child: AdvancedConfigBottomSheet(currentConfig: config),
        ),
      ),
    ));
  }

  testWidgets('hiển thị ngưỡng nước theo %, không còn thời gian bơm', (tester) async {
    await pumpSheet(tester, _config(waterLevelMin: 30));

    expect(find.text('Mực nước tối thiểu: 30%'), findsOneWidget);
    expect(find.textContaining('Thời gian bơm'), findsNothing);
  });

  testWidgets('giá trị cũ ngoài 20–100% không làm hỏng màn hình', (tester) async {
    await pumpSheet(tester, _config(waterLevelMin: 0));

    expect(tester.takeException(), isNull);
    expect(find.text('Mực nước tối thiểu: 20%'), findsOneWidget);
  });

  testWidgets('lưu gửi ngưỡng nước và giữ nguyên thời gian bơm', (tester) async {
    await pumpSheet(tester, _config(waterLevelMin: 0));

    await tester.ensureVisible(find.text('Lưu cài đặt'));
    await tester.tap(find.text('Lưu cài đặt'));
    await tester.pump();

    final event = verify(() => bloc.add(captureAny())).captured.single as AutomationConfigUpdated;
    expect(event.config.waterLevelMin, 20);
    expect(event.config.maxWateringDuration, 2);
  });
}
