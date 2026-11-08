import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../core/network/realtime_client.dart';
import '../core/theme/app_theme.dart';
import '../features/auth/presentation/bloc/auth_bloc.dart';
import '../features/auth/presentation/bloc/auth_event.dart';
import '../features/auth/presentation/bloc/auth_state.dart';
import '../features/automation/presentation/bloc/automation_bloc.dart';
import '../features/automation/presentation/bloc/automation_event.dart';
import '../features/automation/presentation/bloc/automation_state.dart';
import '../features/ble/presentation/bloc/ble_session_cubit.dart';
import '../features/dashboard/presentation/bloc/dashboard_bloc.dart';
import '../features/dashboard/presentation/bloc/dashboard_event.dart';
import '../features/device_management/presentation/bloc/device_bloc.dart';
import '../features/device_management/presentation/bloc/device_event.dart';
import '../features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import '../features/sensor_monitor/presentation/bloc/sensor_event.dart';
import '../features/sensor_monitor/presentation/bloc/statistics_bloc.dart';
import '../injection.dart';
import 'realtime_listener.dart';
import 'routes/app_router.dart';
import 'routes/route_names.dart';

final GlobalKey<ScaffoldMessengerState> _messengerKey =
    GlobalKey<ScaffoldMessengerState>();

class WateringApp extends StatelessWidget {
  const WateringApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiBlocProvider(
      providers: [
        BlocProvider(
          create: (_) => getIt<AuthBloc>()..add(const AuthCheckRequested()),
        ),
        BlocProvider(create: (_) => getIt<DashboardBloc>()),
        BlocProvider(create: (_) => getIt<SensorBloc>()),
        BlocProvider(create: (_) => getIt<StatisticsBloc>()),
        BlocProvider(create: (_) => getIt<DeviceBloc>()),
        BlocProvider(create: (_) => getIt<AutomationBloc>()),
        BlocProvider(create: (_) => getIt<BleSessionCubit>()),
      ],
      child: MaterialApp.router(
        title: 'Smart Watering',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.lightTheme,
        routerConfig: appRouter,
        scaffoldMessengerKey: _messengerKey,
        builder: (context, child) {
          return BlocListener<AutomationBloc, AutomationState>(
            // Giữ trạng thái khóa nút bơm ở trang chủ khớp với công tắc tự động.
            listenWhen: (previous, current) =>
                current is AutomationLoaded &&
                (previous is! AutomationLoaded ||
                    previous.config.autoMode != current.config.autoMode ||
                    previous.config.deviceId != current.config.deviceId),
            listener: (context, state) {
              final config = (state as AutomationLoaded).config;
              context.read<DashboardBloc>().add(DashboardAutoModeLoaded(
                    deviceId: config.deviceId,
                    isAutoMode: config.autoMode,
                  ));
            },
            child: BlocListener<AuthBloc, AuthState>(
            listener: (context, state) {
              if (state is AuthUnauthenticated) {
                context
                    .read<DashboardBloc>()
                    .add(const DashboardResetRequested());
                context.read<SensorBloc>().add(const SensorResetRequested());
                context
                    .read<StatisticsBloc>()
                    .add(const StatisticsResetRequested());
                context
                    .read<AutomationBloc>()
                    .add(const AutomationResetRequested());
                context.read<DeviceBloc>().add(const DeviceResetRequested());
                context.read<BleSessionCubit>().disconnect();
                appRouter.go(RouteNames.login);
              }
            },
            child: RealtimeListener(
              client: getIt<RealtimeClient>(),
              messengerKey: _messengerKey,
              child: child ?? const SizedBox.shrink(),
            ),
            ),
          );
        },
      ),
    );
  }
}
