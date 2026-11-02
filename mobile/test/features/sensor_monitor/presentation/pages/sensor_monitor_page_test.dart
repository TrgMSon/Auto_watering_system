import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/widgets/app_error_widget.dart';
import 'package:watering_app/features/sensor_monitor/domain/entities/sensor_reading.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_event.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_state.dart';
import 'package:watering_app/features/sensor_monitor/presentation/pages/sensor_monitor_page.dart';

class MockSensorBloc extends MockBloc<SensorEvent, SensorState>
    implements SensorBloc {}

void main() {
  late MockSensorBloc mockSensorBloc;

  final testReading = SensorReading(
    id: 1,
    soilMoisture: 45.0,
    temperature: 25.0,
    humidity: 60.0,
    lightIntensity: 0,
    timestamp: DateTime(2026, 11, 2, 10, 0),
  );

  setUpAll(() {
    registerFallbackValue(const SensorRealtimeStarted(deviceId: ''));
    registerFallbackValue(SensorHistoryRequested(
      deviceId: '',
      from: DateTime.now(),
      to: DateTime.now(),
    ));
    registerFallbackValue(const SensorRealtimeStopped());
  });

  setUp(() {
    mockSensorBloc = MockSensorBloc();
  });

  Widget createWidgetUnderTest(String deviceId) {
    return MaterialApp(
      home: BlocProvider<SensorBloc>.value(
        value: mockSensorBloc,
        child: SensorMonitorPage(deviceId: deviceId),
      ),
    );
  }

  group('SensorMonitorPage', () {
    testWidgets(
        'shows "Chưa có thiết bị" and does not call API when deviceId is empty',
        (tester) async {
      when(() => mockSensorBloc.state).thenReturn(const SensorInitial());

      await tester.pumpWidget(createWidgetUnderTest(''));

      expect(find.text('Chưa có thiết bị'), findsOneWidget);
      verifyNever(() => mockSensorBloc.add(any()));
    });

    testWidgets(
        'dispatches SensorHistoryRequested and SensorRealtimeStarted when deviceId is non-empty',
        (tester) async {
      when(() => mockSensorBloc.state).thenReturn(const SensorInitial());

      await tester.pumpWidget(createWidgetUnderTest('ESP_001'));

      verify(() => mockSensorBloc.add(any(
            that: isA<SensorHistoryRequested>().having(
              (e) => e.deviceId,
              'deviceId',
              'ESP_001',
            ),
          ))).called(1);

      verify(() => mockSensorBloc.add(const SensorRealtimeStarted(deviceId: 'ESP_001')))
          .called(1);
    });

    testWidgets('reloads data when deviceId changes in didUpdateWidget',
        (tester) async {
      when(() => mockSensorBloc.state).thenReturn(const SensorInitial());

      await tester.pumpWidget(createWidgetUnderTest('ESP_001'));

      // Rebuild with new deviceId
      await tester.pumpWidget(createWidgetUnderTest('ESP_002'));

      verify(() => mockSensorBloc.add(any(
            that: isA<SensorHistoryRequested>().having(
              (e) => e.deviceId,
              'deviceId',
              'ESP_002',
            ),
          ))).called(1);

      verify(() => mockSensorBloc.add(const SensorRealtimeStarted(deviceId: 'ESP_002')))
          .called(1);
    });

    testWidgets(
        'shows error message line on top while retaining data and charts on SensorLoaded with errorMessage',
        (tester) async {
      when(() => mockSensorBloc.state).thenReturn(SensorLoaded(
        historyReadings: [testReading],
        latestReading: testReading,
        errorMessage: 'Mất kết nối máy chủ',
      ));

      await tester.pumpWidget(createWidgetUnderTest('ESP_001'));

      expect(find.text('Mất kết nối máy chủ'), findsOneWidget);
      expect(find.text('Dữ liệu hiện tại'), findsOneWidget);
      expect(find.text('Biểu đồ 24 giờ qua'), findsOneWidget);
    });

    testWidgets('tapping retry on AppErrorWidget retries with current deviceId',
        (tester) async {
      when(() => mockSensorBloc.state)
          .thenReturn(const SensorError(message: 'Lỗi tải lịch sử'));

      await tester.pumpWidget(createWidgetUnderTest('ESP_001'));

      expect(find.byType(AppErrorWidget), findsOneWidget);
      expect(find.text('Lỗi tải lịch sử'), findsOneWidget);

      await tester.tap(find.text('Thử lại'));
      await tester.pump();

      verify(() => mockSensorBloc.add(any(
            that: isA<SensorHistoryRequested>().having(
              (e) => e.deviceId,
              'deviceId',
              'ESP_001',
            ),
          ))).called(2); // once in initState, once on retry
    });
  });
}
