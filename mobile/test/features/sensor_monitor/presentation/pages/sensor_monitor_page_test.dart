import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_event.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_state.dart';
import 'package:watering_app/features/sensor_monitor/presentation/pages/sensor_monitor_page.dart';

class MockSensorBloc extends MockBloc<SensorEvent, SensorState>
    implements SensorBloc {}

void main() {
  late MockSensorBloc mockSensorBloc;

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
    testWidgets('shows "Chưa có thiết bị" and does not call API when deviceId is empty',
        (tester) async {
      when(() => mockSensorBloc.state).thenReturn(const SensorInitial());

      await tester.pumpWidget(createWidgetUnderTest(''));

      expect(find.text('Chưa có thiết bị'), findsOneWidget);
      verifyNever(() => mockSensorBloc.add(any()));
    });

    testWidgets('dispatches SensorHistoryRequested and SensorRealtimeStarted when deviceId is non-empty',
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
  });
}
