import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/widgets/app_error_widget.dart';
import 'package:watering_app/core/widgets/app_loading_indicator.dart';
import 'package:watering_app/features/dashboard/data/models/device_model.dart';
import 'package:watering_app/features/dashboard/data/models/telemetry_model.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_bloc.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_event.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_state.dart';
import 'package:watering_app/features/dashboard/presentation/pages/dashboard_page.dart';

class MockDashboardBloc extends MockBloc<DashboardEvent, DashboardState>
    implements DashboardBloc {}

void main() {
  late MockDashboardBloc mockDashboardBloc;

  final testDevice1 = DeviceModel(
    id: 'ESP_001',
    deviceName: 'Vườn Lan Ban Công',
    status: 'ONLINE',
    permission: 'OWNER',
    blePassKey: '123456',
  );

  final testDevice2 = DeviceModel(
    id: 'ESP_002',
    deviceName: 'Vườn Hồng Sân Thượng',
    status: 'OFFLINE',
    permission: 'VIEWER',
  );

  final testTelemetryOff = TelemetryModel(
    deviceId: 'ESP_001',
    soilMoisture: 42.5,
    temperature: 28.0,
    humidity: 65.2,
    waterLevel: 78.4,
    isPumpOn: false,
    recordedAt: DateTime(2026, 10, 7, 10, 0),
  );

  final testTelemetryOn = TelemetryModel(
    deviceId: 'ESP_001',
    soilMoisture: 42.5,
    temperature: 28.0,
    humidity: 65.2,
    waterLevel: 78.4,
    isPumpOn: true,
    recordedAt: DateTime(2026, 10, 7, 10, 0),
  );

  setUpAll(() {
    registerFallbackValue(const DashboardLoadRequested());
    registerFallbackValue(DashboardDeviceSelected(testDevice1));
    registerFallbackValue(const DashboardPumpToggled(true));
  });

  setUp(() {
    mockDashboardBloc = MockDashboardBloc();
  });

  Widget buildTestWidget({
    DashboardBloc? bloc,
    ThemeMode themeMode = ThemeMode.light,
  }) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: ThemeData.light(useMaterial3: true),
      darkTheme: ThemeData.dark(useMaterial3: true),
      themeMode: themeMode,
      home: BlocProvider<DashboardBloc>.value(
        value: bloc ?? mockDashboardBloc,
        child: const DashboardPage(),
      ),
    );
  }

  group('DashboardPage Rendering States', () {
    testWidgets('renders AppLoadingIndicator on DashboardLoading state', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(const DashboardLoading());

      await tester.pumpWidget(buildTestWidget());

      expect(find.byType(AppLoadingIndicator), findsOneWidget);
      expect(find.text('Đang tải thiết bị...'), findsOneWidget);
      expect(find.byType(CircularProgressIndicator), findsOneWidget);
      verify(() => mockDashboardBloc.add(const DashboardLoadRequested())).called(1);
    });

    testWidgets('renders AppLoadingIndicator on DashboardInitial state', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(const DashboardInitial());

      await tester.pumpWidget(buildTestWidget());

      expect(find.byType(AppLoadingIndicator), findsOneWidget);
      expect(find.text('Đang tải thiết bị...'), findsOneWidget);
    });

    testWidgets('renders AppErrorWidget with message and retry button on DashboardError state', (tester) async {
      const errorMessage = 'Không thể kết nối đến máy chủ';
      when(() => mockDashboardBloc.state).thenReturn(const DashboardError(message: errorMessage));

      await tester.pumpWidget(buildTestWidget());

      expect(find.byType(AppErrorWidget), findsOneWidget);
      expect(find.text(errorMessage), findsOneWidget);
      expect(find.text('Thử lại'), findsOneWidget);
      expect(find.byIcon(Icons.error_outline), findsOneWidget);
    });

    testWidgets('tapping retry on AppErrorWidget dispatches DashboardLoadRequested', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(const DashboardError(message: 'Lỗi tải dữ liệu'));

      await tester.pumpWidget(buildTestWidget());
      clearInteractions(mockDashboardBloc);

      final retryButton = find.widgetWithText(ElevatedButton, 'Thử lại');
      expect(retryButton, findsOneWidget);

      await tester.tap(retryButton);
      await tester.pumpAndSettle();

      verify(() => mockDashboardBloc.add(const DashboardLoadRequested())).called(1);
    });

    testWidgets('renders empty message when device list is empty', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(const DashboardLoaded(devices: []));

      await tester.pumpWidget(buildTestWidget());

      expect(find.text('Tổng quan'), findsOneWidget);
      expect(find.text('Bạn chưa có thiết bị nào.'), findsOneWidget);
    });

    testWidgets('renders Loaded state: dropdown, pump card, sensor cards, and RefreshIndicator', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1, testDevice2],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
        isBleConnected: false,
      ));

      await tester.pumpWidget(buildTestWidget());

      // Dropdown with selected device
      expect(find.byType(DropdownButton<DeviceModel>), findsOneWidget);
      expect(find.text('Vườn Lan Ban Công'), findsOneWidget);

      // Connection status indicator (WiFi for cloud connection)
      expect(find.byIcon(Icons.wifi), findsOneWidget);
      expect(find.text('ONLINE'), findsOneWidget);

      // RefreshIndicator
      expect(find.byType(RefreshIndicator), findsOneWidget);

      // Pump Card
      expect(find.text('Máy bơm'), findsOneWidget);
      expect(find.text('Đang tắt'), findsOneWidget);
      expect(find.byType(Switch), findsOneWidget);
      final pumpSwitch = tester.widget<Switch>(find.byType(Switch));
      expect(pumpSwitch.value, isFalse);

      // Telemetry Section Title
      expect(find.text('Thông số hiện tại'), findsOneWidget);

      // Sensor cards titles
      expect(find.text('Độ ẩm đất'), findsOneWidget);
      expect(find.text('Nhiệt độ'), findsOneWidget);
      expect(find.text('Độ ẩm K.Khí'), findsOneWidget);
      expect(find.text('Mực nước'), findsOneWidget);

      // Sensor telemetry values
      expect(find.text('42.5%'), findsOneWidget);
      expect(find.text('28.0°C'), findsOneWidget);
      expect(find.text('65.2%'), findsOneWidget);
      expect(find.text('78.4%'), findsOneWidget);

      // Sensor icons
      expect(find.byIcon(Icons.grass), findsOneWidget);
      expect(find.byIcon(Icons.thermostat), findsOneWidget);
      expect(find.byIcon(Icons.cloud), findsOneWidget);
      expect(find.byIcon(Icons.water), findsOneWidget);
    });

    testWidgets('renders Bluetooth icon when isBleConnected is true', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
        isBleConnected: true,
      ));

      await tester.pumpWidget(buildTestWidget());

      expect(find.byIcon(Icons.bluetooth_connected), findsOneWidget);
      expect(find.byIcon(Icons.wifi), findsNothing);
    });

    testWidgets('renders loading spinner in sensor section when telemetry is null', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: null,
      ));

      await tester.pumpWidget(buildTestWidget());

      // Sensor section should show a CircularProgressIndicator instead of the grid
      expect(find.text('Thông số hiện tại'), findsOneWidget);
      expect(find.byType(CircularProgressIndicator), findsOneWidget);
      expect(find.text('Độ ẩm đất'), findsNothing);
    });

    testWidgets('renders loading spinner replacing switch when isTogglingPump is true', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
        isTogglingPump: true,
      ));

      await tester.pumpWidget(buildTestWidget());

      expect(find.byType(Switch), findsNothing);
      expect(find.byType(CircularProgressIndicator), findsOneWidget);
    });

    testWidgets('renders active pump status text when isPumpOn is true', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOn,
      ));

      await tester.pumpWidget(buildTestWidget());

      expect(find.text('Đang tưới nước'), findsOneWidget);
      final pumpSwitch = tester.widget<Switch>(find.byType(Switch));
      expect(pumpSwitch.value, isTrue);
    });
  });

  group('DashboardPage User Interactions', () {
    testWidgets('tapping switch when pump is OFF dispatches DashboardPumpToggled(true)', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget());
      clearInteractions(mockDashboardBloc);

      final switchFinder = find.byType(Switch);
      expect(switchFinder, findsOneWidget);

      await tester.tap(switchFinder);
      await tester.pumpAndSettle();

      verify(() => mockDashboardBloc.add(const DashboardPumpToggled(true))).called(1);
    });

    testWidgets('tapping switch when pump is ON dispatches DashboardPumpToggled(false)', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOn,
      ));

      await tester.pumpWidget(buildTestWidget());
      clearInteractions(mockDashboardBloc);

      final switchFinder = find.byType(Switch);
      expect(switchFinder, findsOneWidget);

      await tester.tap(switchFinder);
      await tester.pumpAndSettle();

      verify(() => mockDashboardBloc.add(const DashboardPumpToggled(false))).called(1);
    });

    testWidgets('selecting device from dropdown dispatches DashboardDeviceSelected', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1, testDevice2],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget());
      clearInteractions(mockDashboardBloc);

      // Open dropdown
      await tester.tap(find.text('Vườn Lan Ban Công'));
      await tester.pumpAndSettle();

      // Find and select second device from dropdown overlay list
      final secondDeviceOption = find.text('Vườn Hồng Sân Thượng').last;
      expect(secondDeviceOption, findsOneWidget);

      await tester.tap(secondDeviceOption);
      await tester.pumpAndSettle();

      verify(() => mockDashboardBloc.add(DashboardDeviceSelected(testDevice2))).called(1);
    });

    testWidgets('pulling to refresh dispatches DashboardLoadRequested', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget());
      clearInteractions(mockDashboardBloc);

      // Perform pull-down gesture to trigger RefreshIndicator
      await tester.fling(find.byType(ListView), const Offset(0.0, 300.0), 1000.0);
      await tester.pump();
      await tester.pump(const Duration(seconds: 1));
      await tester.pump(const Duration(seconds: 1));
      await tester.pumpAndSettle();

      verify(() => mockDashboardBloc.add(const DashboardLoadRequested())).called(1);
    });
  });

  group('DashboardPage UI/UX Mobile Ergonomics & Robustness', () {
    testWidgets('touch target size of interactive Switch is at least 48x48dp', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget());

      final switchFinder = find.byType(Switch);
      final switchSize = tester.getSize(switchFinder);

      expect(switchSize.width, greaterThanOrEqualTo(48.0));
      expect(switchSize.height, greaterThanOrEqualTo(48.0));
    });

    testWidgets('touch target size of retry button is at least 48x48dp', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(const DashboardError(message: 'Error'));

      await tester.pumpWidget(buildTestWidget());

      final retryButton = find.widgetWithText(ElevatedButton, 'Thử lại');
      final buttonSize = tester.getSize(retryButton);

      expect(buttonSize.width, greaterThanOrEqualTo(48.0));
      expect(buttonSize.height, greaterThanOrEqualTo(48.0));
    });

    testWidgets('renders cleanly without overflow on small mobile screen (320x568 dp)', (tester) async {
      // Simulate iPhone SE / narrow Android viewport
      tester.view.physicalSize = const Size(320, 568);
      tester.view.devicePixelRatio = 1.0;
      addTearDown(() {
        tester.view.resetPhysicalSize();
        tester.view.resetDevicePixelRatio();
      });

      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1, testDevice2],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget());
      await tester.pumpAndSettle();

      final exception = tester.takeException();
      expect(exception, isNull);
      expect(find.byType(DashboardPage), findsOneWidget);
      expect(find.text('42.5%'), findsOneWidget);
    });

    testWidgets('renders properly under dark theme', (tester) async {
      when(() => mockDashboardBloc.state).thenReturn(DashboardLoaded(
        devices: [testDevice1],
        selectedDevice: testDevice1,
        telemetry: testTelemetryOff,
      ));

      await tester.pumpWidget(buildTestWidget(themeMode: ThemeMode.dark));
      await tester.pumpAndSettle();

      expect(find.byType(DashboardPage), findsOneWidget);
      expect(find.text('Vườn Lan Ban Công'), findsOneWidget);
      expect(tester.takeException(), isNull);
    });
  });
}
