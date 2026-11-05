import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../core/network/realtime_client.dart';
import '../core/theme/app_colors.dart';
import '../features/auth/presentation/bloc/auth_bloc.dart';
import '../features/auth/presentation/bloc/auth_state.dart';
import '../features/dashboard/presentation/bloc/dashboard_bloc.dart';
import '../features/dashboard/presentation/bloc/dashboard_event.dart';
import '../features/dashboard/presentation/bloc/dashboard_state.dart';
import '../features/device_management/presentation/bloc/device_bloc.dart';
import '../features/device_management/presentation/bloc/device_event.dart';

/// Giữ kết nối WebSocket theo phiên đăng nhập và đưa thông báo vào giao diện.
class RealtimeListener extends StatefulWidget {
  final RealtimeClient client;
  final GlobalKey<ScaffoldMessengerState> messengerKey;
  final Widget child;

  const RealtimeListener({
    super.key,
    required this.client,
    required this.messengerKey,
    required this.child,
  });

  @override
  State<RealtimeListener> createState() => _RealtimeListenerState();
}

class _RealtimeListenerState extends State<RealtimeListener> {
  late final List<StreamSubscription<Object?>> _subscriptions;

  @override
  void initState() {
    super.initState();
    _subscriptions = [
      widget.client.deviceShared.listen(_onDeviceShared),
      widget.client.pumpStatus.listen(_onPumpStatus),
      widget.client.deviceAlerts.listen(_onDeviceAlert),
    ];
    _syncConnection(context.read<AuthBloc>().state);
  }

  @override
  void dispose() {
    for (final subscription in _subscriptions) {
      subscription.cancel();
    }
    super.dispose();
  }

  void _syncConnection(AuthState state) {
    if (state is AuthAuthenticated) {
      widget.client.connect();
    } else if (state is AuthUnauthenticated) {
      widget.client.disconnect();
    }
  }

  void _onDeviceShared(DeviceSharedEvent event) {
    widget.messengerKey.currentState?.showSnackBar(
      SnackBar(content: Text(event.message)),
    );
    context.read<DashboardBloc>().add(const DashboardLoadRequested());
    context.read<DeviceBloc>().add(const DeviceLoadRequested());
  }

  void _onPumpStatus(PumpStatusEvent event) {
    context.read<DashboardBloc>().add(
          DashboardPumpStatusReceived(deviceId: event.deviceId, isOn: event.isOn),
        );
  }

  void _onDeviceAlert(DeviceAlertEvent event) {
    widget.messengerKey.currentState?.showSnackBar(
      SnackBar(
        content: Text(event.message),
        backgroundColor: AppColors.error,
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return MultiBlocListener(
      listeners: [
        BlocListener<AuthBloc, AuthState>(
          listener: (context, state) => _syncConnection(state),
        ),
        BlocListener<DashboardBloc, DashboardState>(
          listener: (context, state) {
            widget.client.watchDevice(
              state is DashboardLoaded ? state.selectedDevice?.id : null,
            );
          },
        ),
      ],
      child: widget.child,
    );
  }
}
