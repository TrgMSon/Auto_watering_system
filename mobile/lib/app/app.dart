import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../core/theme/app_theme.dart';
import '../features/auth/presentation/bloc/auth_bloc.dart';
import '../features/auth/presentation/bloc/auth_event.dart';
import '../features/auth/presentation/bloc/auth_state.dart';
import '../features/automation/presentation/bloc/automation_bloc.dart';
import '../features/automation/presentation/bloc/automation_event.dart';
import '../features/dashboard/presentation/bloc/dashboard_bloc.dart';
import '../features/dashboard/presentation/bloc/dashboard_event.dart';
import '../features/device_management/presentation/bloc/device_bloc.dart';
import '../features/device_management/presentation/bloc/device_event.dart';
import '../features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import '../features/sensor_monitor/presentation/bloc/sensor_event.dart';
import '../injection.dart';
import 'routes/app_router.dart';
import 'routes/route_names.dart';

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
        BlocProvider(create: (_) => getIt<DeviceBloc>()),
        BlocProvider(create: (_) => getIt<AutomationBloc>()),
      ],
      child: MaterialApp.router(
        title: 'Smart Watering',
        debugShowCheckedModeBanner: false,
        theme: AppTheme.lightTheme,
        routerConfig: appRouter,
        builder: (context, child) {
          return BlocListener<AuthBloc, AuthState>(
            listener: (context, state) {
              if (state is AuthUnauthenticated) {
                context
                    .read<DashboardBloc>()
                    .add(const DashboardResetRequested());
                context.read<SensorBloc>().add(const SensorResetRequested());
                context
                    .read<AutomationBloc>()
                    .add(const AutomationResetRequested());
                context.read<DeviceBloc>().add(const DeviceResetRequested());
                appRouter.go(RouteNames.login);
              }
            },
            child: child ?? const SizedBox.shrink(),
          );
        },
      ),
    );
  }
}
