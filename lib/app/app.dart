import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../core/theme/app_theme.dart';
import '../features/auth/presentation/bloc/auth_bloc.dart';
import '../features/auth/presentation/bloc/auth_event.dart';
import '../features/dashboard/presentation/bloc/dashboard_bloc.dart';
import '../features/sensor_monitor/presentation/bloc/sensor_bloc.dart';
import '../features/device_management/presentation/bloc/device_bloc.dart';
import '../features/automation/presentation/bloc/automation_bloc.dart';
import '../injection.dart';
import 'routes/app_router.dart';

class WateringApp extends StatelessWidget {
  const WateringApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MultiBlocProvider(
      providers: [
        BlocProvider(create: (_) => getIt<AuthBloc>()..add(const AuthCheckRequested())),
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
      ),
    );
  }
}
