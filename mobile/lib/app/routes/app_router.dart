import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:go_router/go_router.dart';
import 'route_names.dart';
import '../../features/auth/presentation/bloc/register_bloc.dart';
import '../../features/auth/presentation/pages/login_page.dart';
import '../../features/auth/presentation/pages/register_page.dart';
import '../../features/automation/presentation/pages/automation_page.dart';
import '../../features/dashboard/presentation/bloc/dashboard_bloc.dart';
import '../../features/dashboard/presentation/bloc/dashboard_state.dart';
import '../../features/dashboard/presentation/pages/dashboard_page.dart';
import '../../features/device_management/presentation/pages/device_list_page.dart';
import '../../features/profile/presentation/pages/profile_page.dart';
import '../../features/sensor_monitor/presentation/pages/sensor_monitor_page.dart';
import '../../features/sharing/presentation/bloc/sharing_bloc.dart';
import '../../features/sharing/presentation/bloc/sharing_event.dart';
import '../../features/sharing/presentation/pages/family_guests_page.dart';
import '../../injection.dart';

final GlobalKey<NavigatorState> rootNavigatorKey = GlobalKey<NavigatorState>();

String? _selectSelectedDeviceId(BuildContext context) {
  return context.select<DashboardBloc, String?>((bloc) {
    final dashboardState = bloc.state;
    if (dashboardState is! DashboardLoaded) return null;
    return dashboardState.selectedDevice?.id ??
        (dashboardState.devices.isNotEmpty
            ? dashboardState.devices.first.id
            : null);
  });
}

final GoRouter appRouter = GoRouter(
  navigatorKey: rootNavigatorKey,
  initialLocation: RouteNames.login,
  routes: [
    GoRoute(
      path: RouteNames.login,
      builder: (context, state) => const LoginPage(),
    ),
    GoRoute(
      path: RouteNames.register,
      builder: (context, state) => BlocProvider(
        create: (_) => getIt<RegisterBloc>(),
        child: const RegisterPage(),
      ),
    ),
    StatefulShellRoute.indexedStack(
      builder: (context, state, navigationShell) {
        return _ScaffoldWithBottomNav(navigationShell: navigationShell);
      },
      branches: [
        StatefulShellBranch(
          routes: [
            GoRoute(
              path: RouteNames.dashboard,
              builder: (context, state) => const DashboardPage(),
            ),
          ],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(
              path: RouteNames.analytics,
              builder: (context, state) {
                final deviceId = _selectSelectedDeviceId(context);
                final isPumpOn = context.select<DashboardBloc, bool>((bloc) {
                  final dashboardState = bloc.state;
                  return dashboardState is DashboardLoaded &&
                      (dashboardState.pumpStatus ?? dashboardState.telemetry?.isPumpOn) == true;
                });
                return AutomationPage(deviceId: deviceId ?? '', isPumpOn: isPumpOn);
              },
            ),
          ],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(
              path: RouteNames.monitor,
              builder: (context, state) {
                final deviceId = _selectSelectedDeviceId(context);
                return SensorMonitorPage(deviceId: deviceId ?? '');
              },
            ),
          ],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(
              path: RouteNames.settings,
              builder: (context, state) => const ProfilePage(),
              routes: [
                GoRoute(
                  path: 'devices',
                  parentNavigatorKey: rootNavigatorKey,
                  builder: (context, state) => const DeviceListPage(),
                ),
                GoRoute(
                  path: 'family',
                  parentNavigatorKey: rootNavigatorKey,
                  builder: (context, state) => BlocProvider(
                    create: (_) =>
                        getIt<SharingBloc>()..add(const SharingLoadRequested()),
                    child: const FamilyGuestsPage(),
                  ),
                ),
              ],
            ),
          ],
        ),
      ],
    ),
  ],
);

class _ScaffoldWithBottomNav extends StatelessWidget {
  final StatefulNavigationShell navigationShell;

  const _ScaffoldWithBottomNav({required this.navigationShell});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: navigationShell,
      bottomNavigationBar: NavigationBar(
        selectedIndex: navigationShell.currentIndex,
        onDestinationSelected: (index) => navigationShell.goBranch(
          index,
          initialLocation: index == navigationShell.currentIndex,
        ),
        destinations: const [
          NavigationDestination(
            icon: Icon(Icons.dashboard_outlined),
            selectedIcon: Icon(Icons.dashboard),
            label: 'Tổng quan',
          ),
          NavigationDestination(
            icon: Icon(Icons.auto_awesome_outlined),
            selectedIcon: Icon(Icons.auto_awesome),
            label: 'Tự động hóa',
          ),
          NavigationDestination(
            icon: Icon(Icons.show_chart),
            selectedIcon: Icon(Icons.show_chart),
            label: 'Biểu đồ',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'Cài đặt',
          ),
        ],
      ),
    );
  }
}
