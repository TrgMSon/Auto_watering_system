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

  setUp(() {
    mockDeviceBloc = MockDeviceBloc();
  });

  Widget buildWidget() {
    return MaterialApp(
      home: BlocProvider<DeviceBloc>.value(
        value: mockDeviceBloc,
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
  });
}
