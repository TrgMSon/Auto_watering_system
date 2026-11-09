import 'dart:async';
import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_state.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_bloc.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_event.dart';
import 'package:watering_app/features/automation/presentation/bloc/automation_state.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_bloc.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_event.dart';
import 'package:watering_app/features/dashboard/presentation/bloc/dashboard_state.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_bloc.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_event.dart';
import 'package:watering_app/features/device_management/presentation/bloc/device_state.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_event.dart';
import 'package:watering_app/features/sensor_monitor/presentation/bloc/sensor_state.dart';

class MockAuthBloc extends MockBloc<AuthEvent, AuthState> implements AuthBloc {}
class MockDashboardBloc extends MockBloc<DashboardEvent, DashboardState> implements DashboardBloc {}
class MockSensorBloc extends MockBloc<SensorEvent, SensorState> implements SensorBloc {}
class MockAutomationBloc extends MockBloc<AutomationEvent, AutomationState> implements AutomationBloc {}
class MockDeviceBloc extends MockBloc<DeviceEvent, DeviceState> implements DeviceBloc {}

void main() {
  late MockAuthBloc mockAuthBloc;
  late MockDashboardBloc mockDashboardBloc;
  late MockSensorBloc mockSensorBloc;
  late MockAutomationBloc mockAutomationBloc;
  late MockDeviceBloc mockDeviceBloc;
  late StreamController<AuthState> authStateController;

  setUp(() {
    mockAuthBloc = MockAuthBloc();
    mockDashboardBloc = MockDashboardBloc();
    mockSensorBloc = MockSensorBloc();
    mockAutomationBloc = MockAutomationBloc();
    mockDeviceBloc = MockDeviceBloc();
    authStateController = StreamController<AuthState>.broadcast();

    when(() => mockAuthBloc.stream).thenAnswer((_) => authStateController.stream);
    when(() => mockAuthBloc.state).thenReturn(const AuthInitial());
    when(() => mockDashboardBloc.state).thenReturn(const DashboardInitial());
    when(() => mockSensorBloc.state).thenReturn(const SensorInitial());
    when(() => mockAutomationBloc.state).thenReturn(const AutomationInitial());
    when(() => mockDeviceBloc.state).thenReturn(const DeviceInitial());
  });

  tearDown(() {
    authStateController.close();
  });

  testWidgets('resets DashboardBloc, SensorBloc, AutomationBloc and DeviceBloc when AuthUnauthenticated is emitted', (tester) async {
    await tester.pumpWidget(
      MultiBlocProvider(
        providers: [
          BlocProvider<AuthBloc>.value(value: mockAuthBloc),
          BlocProvider<DashboardBloc>.value(value: mockDashboardBloc),
          BlocProvider<SensorBloc>.value(value: mockSensorBloc),
          BlocProvider<AutomationBloc>.value(value: mockAutomationBloc),
          BlocProvider<DeviceBloc>.value(value: mockDeviceBloc),
        ],
        child: MaterialApp(
          home: BlocListener<AuthBloc, AuthState>(
            listener: (context, state) {
              if (state is AuthUnauthenticated) {
                context.read<DashboardBloc>().add(const DashboardResetRequested());
                context.read<SensorBloc>().add(const SensorResetRequested());
                context.read<AutomationBloc>().add(const AutomationResetRequested());
                context.read<DeviceBloc>().add(const DeviceResetRequested());
              }
            },
            child: const Scaffold(body: SizedBox.shrink()),
          ),
        ),
      ),
    );

    authStateController.add(const AuthUnauthenticated());
    await tester.pump();

    verify(() => mockDashboardBloc.add(const DashboardResetRequested())).called(1);
    verify(() => mockSensorBloc.add(const SensorResetRequested())).called(1);
    verify(() => mockAutomationBloc.add(const AutomationResetRequested())).called(1);
    verify(() => mockDeviceBloc.add(const DeviceResetRequested())).called(1);
  });
}

