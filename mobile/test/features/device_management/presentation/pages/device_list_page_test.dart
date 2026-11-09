import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/widgets/app_error_widget.dart';
import 'package:watering_app/core/widgets/app_loading_indicator.dart';
import 'package:watering_app/core/widgets/empty_device_widget.dart';
import 'package:watering_app/features/device_management/domain/entities/device.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_bloc.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_event.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_state.dart';
import 'package:watering_app/features/device_management/presentation/pages/device_list_page.dart';
import 'package:watering_app/features/device_management/presentation/widgets/device_card.dart';
import 'package:watering_app/features/ble/presentation/bloc/ble_session_cubit.dart';

class MockBleSessionCubit extends MockCubit<BleSessionState> implements BleSessionCubit {}

class MockDeviceBloc extends MockBloc<DeviceEvent, DeviceState>
    implements DeviceBloc {}

void main() {
  late MockDeviceBloc mockDeviceBloc;

  const testDevice = Device(
    id: 'ESP_001',
    name: 'Vườn Lan Ban Công',
    type: DeviceType.sensor,
    isOnline: true,
  );

  setUpAll(() {
    registerFallbackValue(const DeviceLoadRequested());
  });

  late MockBleSessionCubit mockBleCubit;

  setUp(() {
    mockDeviceBloc = MockDeviceBloc();
    mockBleCubit = MockBleSessionCubit();
    when(() => mockBleCubit.state).thenReturn(const BleSessionState());
  });

  Widget buildWidget() {
    return MaterialApp(
      home: MultiBlocProvider(
        providers: [
          BlocProvider<DeviceBloc>.value(value: mockDeviceBloc),
          BlocProvider<BleSessionCubit>.value(value: mockBleCubit),
        ],
        child: const DeviceListPage(),
      ),
    );
  }

  group('DeviceListPage', () {
    testWidgets('shows loading indicator on DeviceLoading state', (tester) async {
      when(() => mockDeviceBloc.state).thenReturn(const DeviceLoading());

      await tester.pumpWidget(buildWidget());

      expect(find.byType(AppLoadingIndicator), findsOneWidget);
      expect(find.text('Đang tải danh sách thiết bị...'), findsOneWidget);
      verify(() => mockDeviceBloc.add(const DeviceLoadRequested())).called(1);
    });

    testWidgets('shows AppErrorWidget and dispatches DeviceLoadRequested on retry', (tester) async {
      when(() => mockDeviceBloc.state).thenReturn(const DeviceError(message: 'Lỗi kết nối'));

      await tester.pumpWidget(buildWidget());

      expect(find.byType(AppErrorWidget), findsOneWidget);
      expect(find.text('Lỗi kết nối'), findsOneWidget);

      await tester.tap(find.text('Thử lại'));
      await tester.pump();

      verify(() => mockDeviceBloc.add(const DeviceLoadRequested())).called(2);
    });

    testWidgets('renders EmptyDeviceWidget and dispatches DeviceLoadRequested on reload', (tester) async {
      when(() => mockDeviceBloc.state).thenReturn(const DeviceLoaded(devices: []));

      await tester.pumpWidget(buildWidget());

      expect(find.byType(EmptyDeviceWidget), findsOneWidget);
      expect(find.byIcon(Icons.devices_other), findsOneWidget);
      expect(find.text('Bạn chưa có thiết bị nào.'), findsOneWidget);
      expect(find.text('Liên hệ quản trị viên để được gán thiết bị'), findsOneWidget);
      expect(find.widgetWithText(ElevatedButton, 'Tải lại'), findsOneWidget);

      await tester.tap(find.widgetWithText(ElevatedButton, 'Tải lại'));
      await tester.pump();

      verify(() => mockDeviceBloc.add(const DeviceLoadRequested())).called(2);
    });

    testWidgets('renders device list when devices are available', (tester) async {
      when(() => mockDeviceBloc.state).thenReturn(const DeviceLoaded(devices: [testDevice]));

      await tester.pumpWidget(buildWidget());

      expect(find.byType(DeviceCard), findsOneWidget);
      expect(find.text('Vườn Lan Ban Công'), findsOneWidget);
    });

    group('Wi-Fi form', () {
      final ssidField = find.widgetWithText(TextFormField, 'Tên Wi-Fi');
      final passwordField = find.widgetWithText(TextFormField, 'Mật khẩu Wi-Fi');
      final submitButton = find.widgetWithText(ElevatedButton, 'Gửi tới thiết bị');

      Future<void> openForm(WidgetTester tester) async {
        when(() => mockDeviceBloc.state).thenReturn(const DeviceLoaded(devices: [testDevice]));
        await tester.pumpWidget(buildWidget());
        await tester.tap(find.byType(DeviceCard));
        await tester.pumpAndSettle();
      }

      testWidgets('opens when a device is tapped', (tester) async {
        await openForm(tester);

        expect(find.text('Cấu hình Wi-Fi'), findsOneWidget);
        expect(ssidField, findsOneWidget);
        expect(passwordField, findsOneWidget);
        expect(find.text('Vườn Lan Ban Công'), findsNWidgets(2));
      });

      testWidgets('requires a Wi-Fi name and password', (tester) async {
        await openForm(tester);

        await tester.tap(submitButton);
        await tester.pump();

        expect(find.text('Vui lòng nhập tên Wi-Fi'), findsOneWidget);
        expect(find.text('Vui lòng nhập mật khẩu Wi-Fi'), findsOneWidget);
      });

      testWidgets('rejects a name over 32 bytes and a short password', (tester) async {
        await openForm(tester);

        await tester.enterText(ssidField, 'đ' * 17);
        await tester.enterText(passwordField, '1234567');
        await tester.tap(submitButton);
        await tester.pump();

        expect(find.text('Tên Wi-Fi quá dài'), findsOneWidget);
        expect(find.text('Mật khẩu phải có từ 8 đến 63 ký tự'), findsOneWidget);
      });

      testWidgets('hides the password until the user reveals it', (tester) async {
        await openForm(tester);

        bool isObscured() => tester
            .widget<TextField>(
              find.descendant(of: passwordField, matching: find.byType(TextField)),
            )
            .obscureText;

        expect(isObscured(), isTrue);

        await tester.tap(find.byIcon(Icons.visibility_off));
        await tester.pump();

        expect(isObscured(), isFalse);
      });

      testWidgets('sends valid input to the device over BLE', (tester) async {
        when(() => mockBleCubit.sendWifi(any(), ssid: any(named: 'ssid'), password: any(named: 'password')))
            .thenAnswer((_) async => null);
        await openForm(tester);

        await tester.enterText(ssidField, 'Nhà Lan');
        await tester.enterText(passwordField, 'password123');
        await tester.tap(submitButton);
        await tester.pumpAndSettle();

        expect(find.text('Cấu hình Wi-Fi'), findsNothing);
        expect(find.text('Thiết bị đã kết nối Wi-Fi.'), findsOneWidget);
        verify(() => mockBleCubit.sendWifi(any(), ssid: 'Nhà Lan', password: 'password123')).called(1);
      });

      testWidgets('shows the error when sending Wi-Fi fails', (tester) async {
        when(() => mockBleCubit.sendWifi(any(), ssid: any(named: 'ssid'), password: any(named: 'password')))
            .thenAnswer((_) async => 'Thiết bị không vào được Wi-Fi. Kiểm tra tên và mật khẩu.');
        await openForm(tester);

        await tester.enterText(ssidField, 'Nhà Lan');
        await tester.enterText(passwordField, 'password123');
        await tester.tap(submitButton);
        await tester.pumpAndSettle();

        expect(find.text('Thiết bị không vào được Wi-Fi. Kiểm tra tên và mật khẩu.'), findsOneWidget);
      });

      testWidgets('rejects the pipe character the firmware cannot sign', (tester) async {
        await openForm(tester);

        await tester.enterText(ssidField, 'Nha|Lan');
        await tester.enterText(passwordField, 'password123');
        await tester.tap(submitButton);
        await tester.pump();

        expect(find.text('Tên Wi-Fi không được chứa dấu |'), findsOneWidget);
        verifyNever(() => mockBleCubit.sendWifi(any(), ssid: any(named: 'ssid'), password: any(named: 'password')));
      });
    });
  });
}
