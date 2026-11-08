import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_bloc.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_event.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_state.dart';
import 'package:watering_app/features/automation/presentation/pages/automation_page.dart';

class MockAutomationBloc extends MockBloc<AutomationEvent, AutomationState>
    implements AutomationBloc {}

DeviceConfigModel _config({required bool autoMode}) => DeviceConfigModel(
      deviceId: 'ESP_001',
      autoMode: autoMode,
      soilMoistureMin: 30,
      soilMoistureMax: 70,
      airHumidityMin: 40,
      airHumidityMax: 80,
      temperatureMin: 20,
      temperatureMax: 35,
      waterLevelMin: 25,
    );

void main() {
  late MockAutomationBloc bloc;

  setUp(() {
    bloc = MockAutomationBloc();
  });

  Future<void> pumpPage(WidgetTester tester, {required bool autoMode, bool isPumpOn = false}) async {
    whenListen(
      bloc,
      const Stream<AutomationState>.empty(),
      initialState: AutomationLoaded(config: _config(autoMode: autoMode), schedules: const []),
    );
    await tester.pumpWidget(MaterialApp(
      home: BlocProvider<AutomationBloc>.value(
        value: bloc,
        child: AutomationPage(deviceId: 'ESP_001', isPumpOn: isPumpOn),
      ),
    ));
  }

  ListTile thresholdTile(WidgetTester tester) => tester.widget<ListTile>(
        find.ancestor(of: find.text('Tùy chỉnh ngưỡng tự động'), matching: find.byType(ListTile)),
      );

  testWidgets('cho sửa ngưỡng khi thủ công và bơm tắt', (tester) async {
    await pumpPage(tester, autoMode: false);

    expect(thresholdTile(tester).enabled, isTrue);
    expect(find.textContaining('Nước tối thiểu: 25%'), findsOneWidget);
    expect(find.textContaining('Thời gian bơm'), findsNothing);
  });

  testWidgets('khóa sửa ngưỡng khi đang tự động', (tester) async {
    await pumpPage(tester, autoMode: true);

    expect(thresholdTile(tester).enabled, isFalse);
    expect(find.textContaining('Tắt tưới tự động để sửa ngưỡng.'), findsOneWidget);
  });

  testWidgets('khóa sửa ngưỡng khi bơm đang chạy', (tester) async {
    await pumpPage(tester, autoMode: false, isPumpOn: true);

    expect(thresholdTile(tester).enabled, isFalse);
    expect(find.textContaining('Chờ bơm tắt để sửa ngưỡng.'), findsOneWidget);
  });
}
