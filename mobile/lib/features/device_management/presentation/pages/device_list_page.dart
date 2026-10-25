import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
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
      appBar: AppBar(title: const Text('Qu\u1ea3n l\u00fd thi\u1ebft b\u1ecb')),
      body: BlocBuilder<DeviceBloc, DeviceState>(
        builder: (context, state) {
          if (state is DeviceInitial || state is DeviceLoading) {
            return const AppLoadingIndicator(message: '\u0110ang t\u1ea3i danh s\u00e1ch thi\u1ebft b\u1ecb...');
          }
          if (state is DeviceError) {
            return AppErrorWidget(
              message: state.message,
              onRetry: () => context.read<DeviceBloc>().add(const DeviceLoadRequested()),
            );
          }
          if (state is DeviceLoaded) {
            if (state.devices.isEmpty) {
              return const Center(
                child: Column(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.devices_other, size: 64, color: AppColors.deviceOffline),
                    SizedBox(height: 16),
                    Text('Ch\u01b0a c\u00f3 thi\u1ebft b\u1ecb n\u00e0o'),
                  ],
                ),
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
