import 'package:flutter/material.dart';
import 'package:go_router/go_router.dart';
import 'route_names.dart';
import '../../features/auth/presentation/pages/login_page.dart';
import '../../features/dashboard/presentation/pages/dashboard_page.dart';
import '../../features/sensor_monitor/presentation/pages/sensor_monitor_page.dart';
import '../../features/device_management/presentation/pages/device_list_page.dart';
import '../../features/profile/presentation/pages/profile_page.dart';
import '../../features/user_management/presentation/pages/user_management_page.dart';
import '../../core/theme/app_colors.dart';

final GlobalKey<NavigatorState> rootNavigatorKey = GlobalKey<NavigatorState>();

final GoRouter appRouter = GoRouter(
  navigatorKey: rootNavigatorKey,
  initialLocation: RouteNames.login,
  routes: [
    GoRoute(
      path: RouteNames.login,
      builder: (context, state) => const LoginPage(),
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
              builder: (context, state) => const SensorMonitorPage(),
            ),
          ],
        ),
        StatefulShellBranch(
          routes: [
            GoRoute(
              path: RouteNames.devices,
              builder: (context, state) => const DeviceListPage(),
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
                  path: 'users',
                  parentNavigatorKey: rootNavigatorKey,
                  builder: (context, state) => const UserManagementPage(),
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
            label: 'T\u1ed5ng quan',
          ),
          NavigationDestination(
            icon: Icon(Icons.show_chart_outlined),
            selectedIcon: Icon(Icons.show_chart),
            label: 'Th\u1ed1ng k\u00ea',
          ),
          NavigationDestination(
            icon: Icon(Icons.devices_other_outlined),
            selectedIcon: Icon(Icons.devices_other),
            label: 'Thi\u1ebft b\u1ecb',
          ),
          NavigationDestination(
            icon: Icon(Icons.settings_outlined),
            selectedIcon: Icon(Icons.settings),
            label: 'C\u00e0i \u0111\u1eb7t',
          ),
        ],
      ),
    );
  }
}
