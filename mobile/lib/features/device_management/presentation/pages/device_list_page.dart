import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../../../core/widgets/empty_device_widget.dart';
import '../bloc/device_bloc.dart';
import '../bloc/device_event.dart';
import '../bloc/device_state.dart';
import '../widgets/device_card.dart';

class DeviceListPage extends StatefulWidget {
  const DeviceListPage({super.key});

  @override
  State<DeviceListPage> createState() => _DeviceListPageState();
}

class _DeviceListPageState extends State<DeviceListPage> {
  @override
  void initState() {
    super.initState();
    context.read<DeviceBloc>().add(const DeviceLoadRequested());
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Quản lý thiết bị')),
      body: BlocBuilder<DeviceBloc, DeviceState>(
        builder: (context, state) {
          if (state is DeviceInitial || state is DeviceLoading) {
            return const AppLoadingIndicator(
              message: 'Đang tải danh sách thiết bị...',
            );
          }
          if (state is DeviceError) {
            return AppErrorWidget(
              message: state.message,
              onRetry: () =>
                  context.read<DeviceBloc>().add(const DeviceLoadRequested()),
            );
          }
          if (state is DeviceLoaded) {
            if (state.devices.isEmpty) {
              return EmptyDeviceWidget(
                onReload: () =>
                    context.read<DeviceBloc>().add(const DeviceLoadRequested()),
              );
            }
            return RefreshIndicator(
              onRefresh: () async {
                context.read<DeviceBloc>().add(const DeviceLoadRequested());
              },
              child: ListView.builder(
                padding: const EdgeInsets.all(16),
                itemCount: state.devices.length,
                itemBuilder: (context, index) {
                  final device = state.devices[index];
                  return Padding(
                    padding: const EdgeInsets.only(bottom: 8),
                    child: DeviceCard(device: device),
                  );
                },
              ),
            );
          }
          return const SizedBox.shrink();
        },
      ),
    );
  }
}
