import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/widgets/app_error_widget.dart';
import 'package:watering_app/core/widgets/app_loading_indicator.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_bloc.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_event.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_state.dart';
import 'package:watering_app/features/automation/presentation/pages/automation_page.dart';

class MockAutomationBloc extends MockBloc<AutomationEvent, AutomationState>
    implements AutomationBloc {}

void main() {
  late MockAutomationBloc mockAutomationBloc;

  const testConfig = DeviceConfigModel(
    deviceId: 'ESP_001',
    autoMode: true,
    soilMoistureMin: 30.0,
    soilMoistureMax: 70.0,
    airHumidityMin: 40.0,
    airHumidityMax: 80.0,
    temperatureMin: 20.0,
    temperatureMax: 35.0,
    waterLevelMin: 15,
  );

  setUpAll(() {
    registerFallbackValue(const AutomationLoadRequested('ESP_001'));
  });

  setUp(() {
    mockAutomationBloc = MockAutomationBloc();
  });

  Widget buildWidget({String deviceId = 'ESP_001'}) {
    return MaterialApp(
      home: BlocProvider<AutomationBloc>.value(
        value: mockAutomationBloc,
        child: AutomationPage(deviceId: deviceId),
      ),
    );
  }

  group('AutomationPage', () {
    testWidgets('shows AppLoadingIndicator when state is AutomationLoading', (tester) async {
      when(() => mockAutomationBloc.state).thenReturn(const AutomationLoading());

      await tester.pumpWidget(buildWidget());

      expect(find.byType(AppLoadingIndicator), findsOneWidget);
      expect(find.text('Đang tải cấu hình tự động...'), findsOneWidget);
      verify(() => mockAutomationBloc.add(const AutomationLoadRequested('ESP_001'))).called(1);
    });

    testWidgets('shows AppErrorWidget and dispatches AutomationLoadRequested on retry', (tester) async {
      when(() => mockAutomationBloc.state).thenReturn(const AutomationError('Lỗi kết nối máy chủ'));

      await tester.pumpWidget(buildWidget());

      expect(find.byType(AppErrorWidget), findsOneWidget);
      expect(find.text('Lỗi kết nối máy chủ'), findsOneWidget);
      expect(find.text('Thử lại'), findsOneWidget);

      await tester.tap(find.text('Thử lại'));
      await tester.pump();

      verify(() => mockAutomationBloc.add(const AutomationLoadRequested('ESP_001'))).called(2);
    });

    testWidgets('shows config and schedule UI when state is AutomationLoaded', (tester) async {
      when(() => mockAutomationBloc.state).thenReturn(const AutomationLoaded(
        config: testConfig,
        schedules: [],
      ));

      await tester.pumpWidget(buildWidget());

      expect(find.text('Chế độ cảm biến'), findsOneWidget);
      expect(find.text('Bật tưới tự động'), findsOneWidget);
      expect(find.text('Lịch hẹn giờ'), findsOneWidget);
      expect(find.text('Chưa có lịch hẹn giờ nào.'), findsOneWidget);
    });
  });
}
