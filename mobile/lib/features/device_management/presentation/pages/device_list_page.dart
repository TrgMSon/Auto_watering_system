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
      floatingActionButton: FloatingActionButton(
        onPressed: () => _showAddDeviceDialog(context),
        backgroundColor: AppColors.primary,
        child: const Icon(Icons.add, color: Colors.white),
      ),
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
                    child: DeviceCard(
                      device: device,
                      onToggle: (activate) {
                        context.read<DeviceBloc>().add(
                          DeviceToggleRequested(
                            deviceId: device.id,
                            activate: activate,
                          ),
                        );
                      },
                      onDelete: () {
                        showDialog(
                          context: context,
                          builder: (ctx) => AlertDialog(
                            title: const Text('X\u00e1c nh\u1eadn x\u00f3a'),
                            content: Text('B\u1ea1n c\u00f3 ch\u1eafc ch\u1eafn mu\u1ed1n x\u00f3a \"\${device.name}\"?'),
                            actions: [
                              TextButton(
                                onPressed: () => Navigator.of(ctx).pop(),
                                child: const Text('H\u1ee7y'),
                              ),
                              FilledButton(
                                onPressed: () {
                                  Navigator.of(ctx).pop();
                                  context.read<DeviceBloc>().add(
                                    DeviceDeleteRequested(deviceId: device.id),
                                  );
                                },
                                style: FilledButton.styleFrom(
                                  backgroundColor: AppColors.error,
                                ),
                                child: const Text('X\u00f3a'),
                              ),
                            ],
                          ),
                        );
                      },
                    ),
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

  void _showAddDeviceDialog(BuildContext context) {
    final nameController = TextEditingController();
    final locationController = TextEditingController();
    String selectedType = 'SENSOR';

    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Th\u00eam thi\u1ebft b\u1ecb'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameController,
              decoration: const InputDecoration(labelText: 'T\u00ean thi\u1ebft b\u1ecb'),
            ),
            const SizedBox(height: 12),
            TextField(
              controller: locationController,
              decoration: const InputDecoration(labelText: 'V\u1ecb tr\u00ed (t\u00f9y ch\u1ecdn)'),
            ),
            const SizedBox(height: 12),
            DropdownButtonFormField<String>(
              value: selectedType,
              items: const [
                DropdownMenuItem(value: 'SENSOR', child: Text('C\u1ea3m bi\u1ebfn')),
                DropdownMenuItem(value: 'PUMP', child: Text('M\u00e1y b\u01a1m')),
                DropdownMenuItem(value: 'VALVE', child: Text('Van')),
                DropdownMenuItem(value: 'GATEWAY', child: Text('Gateway')),
              ],
              onChanged: (v) => selectedType = v ?? 'SENSOR',
              decoration: const InputDecoration(labelText: 'Lo\u1ea1i thi\u1ebft b\u1ecb'),
            ),
          ],
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('H\u1ee7y'),
          ),
          FilledButton(
            onPressed: () {
              if (nameController.text.trim().isNotEmpty) {
                Navigator.of(ctx).pop();
                context.read<DeviceBloc>().add(
                  DeviceAddRequested(
                    name: nameController.text.trim(),
                    type: selectedType,
                    location: locationController.text.trim().isEmpty
                        ? null
                        : locationController.text.trim(),
                  ),
                );
              }
            },
            child: const Text('Th\u00eam'),
          ),
        ],
      ),
    );
  }
}
