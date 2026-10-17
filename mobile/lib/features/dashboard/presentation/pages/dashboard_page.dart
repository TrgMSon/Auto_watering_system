import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../bloc/dashboard_bloc.dart';
import '../bloc/dashboard_event.dart';
import '../bloc/dashboard_state.dart';
import '../../data/models/device_model.dart';

class DashboardPage extends StatefulWidget {
  const DashboardPage({super.key});

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  @override
  void initState() {
    super.initState();
    context.read<DashboardBloc>().add(const DashboardLoadRequested());
  }

  @override
  Widget build(BuildContext context) {
    return BlocBuilder<DashboardBloc, DashboardState>(
      builder: (context, state) {
        if (state is DashboardInitial || state is DashboardLoading) {
          return const Scaffold(
            body: AppLoadingIndicator(message: 'Đang tải thiết bị...'),
          );
        }
        
        if (state is DashboardError) {
          return Scaffold(
            body: AppErrorWidget(
              message: state.message,
              onRetry: () => context.read<DashboardBloc>().add(const DashboardLoadRequested()),
            ),
          );
        }
        
        if (state is DashboardLoaded) {
          if (state.devices.isEmpty) {
            return Scaffold(
              appBar: AppBar(title: const Text('Tổng quan')),
              body: const Center(child: Text('Bạn chưa có thiết bị nào.')),
            );
          }
          
          final selectedDevice = state.selectedDevice ?? state.devices.first;
          final telemetry = state.telemetry;

          return Scaffold(
            appBar: AppBar(
              title: DropdownButtonHideUnderline(
                child: DropdownButton<DeviceModel>(
                  isExpanded: true,
                  value: selectedDevice,
                  icon: const Icon(Icons.arrow_drop_down, color: Colors.white),
                  dropdownColor: AppColors.primary,
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 18,
                    fontWeight: FontWeight.w600,
                  ),
                  onChanged: (DeviceModel? newValue) {
                    if (newValue != null) {
                      context.read<DashboardBloc>().add(DashboardDeviceSelected(newValue));
                    }
                  },
                  items: state.devices.map<DropdownMenuItem<DeviceModel>>((DeviceModel device) {
                    return DropdownMenuItem<DeviceModel>(
                      value: device,
                      child: Text(
                        device.deviceName,
                        overflow: TextOverflow.ellipsis,
                      ),
                    );
                  }).toList(),
                ),
              ),
              actions: [
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16.0),
                  child: Row(
                    children: [
                      Icon(
                        state.isBleConnected ? Icons.bluetooth_connected : Icons.wifi,
                        color: state.isBleConnected ? Colors.blueAccent : Colors.greenAccent,
                        size: 20,
                      ),
                      const SizedBox(width: 8),
                      Text(
                        selectedDevice.status,
                        style: const TextStyle(fontSize: 12),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            body: RefreshIndicator(
              onRefresh: () async {
                context.read<DashboardBloc>().add(const DashboardLoadRequested());
              },
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  // Manual Pump Control Card
                  Card(
                    child: Padding(
                      padding: const EdgeInsets.all(16),
                      child: Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Expanded(
                            child: Row(
                              children: [
                                Container(
                                  padding: const EdgeInsets.all(12),
                                  decoration: BoxDecoration(
                                    color: AppColors.primary.withValues(alpha: 0.1),
                                    shape: BoxShape.circle,
                                  ),
                                  child: const Icon(Icons.water_drop, color: AppColors.primary),
                                ),
                                const SizedBox(width: 16),
                                Expanded(
                                  child: Column(
                                    crossAxisAlignment: CrossAxisAlignment.start,
                                    children: [
                                      Text(
                                        'Máy bơm',
                                        style: Theme.of(context).textTheme.titleMedium,
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                      ),
                                      Text(
                                        telemetry?.isPumpOn == true ? 'Đang tưới nước' : 'Đang tắt',
                                        maxLines: 1,
                                        overflow: TextOverflow.ellipsis,
                                        style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                          color: telemetry?.isPumpOn == true ? AppColors.primary : AppColors.textSecondary,
                                        ),
                                      ),
                                    ],
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(width: 8),
                          state.isTogglingPump
                              ? const SizedBox(
                                  width: 48,
                                  height: 48,
                                  child: Padding(
                                    padding: EdgeInsets.all(14.0),
                                    child: CircularProgressIndicator(strokeWidth: 2),
                                  ),
                                )
                              : Switch(
                                  value: telemetry?.isPumpOn ?? false,
                                  onChanged: (value) {
                                    context.read<DashboardBloc>().add(DashboardPumpToggled(value));
                                  },
                                ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  
                  // Sensor Grid
                  Text('Thông số hiện tại', style: Theme.of(context).textTheme.titleMedium),
                  const SizedBox(height: 16),
                  
                  if (telemetry == null)
                    const Center(child: Padding(
                      padding: EdgeInsets.all(32.0),
                      child: CircularProgressIndicator(),
                    ))
                  else
                    GridView.count(
                      crossAxisCount: 2,
                      shrinkWrap: true,
                      physics: const NeverScrollableScrollPhysics(),
                      mainAxisSpacing: 16,
                      crossAxisSpacing: 16,
                      childAspectRatio: 1.2,
                      children: [
                        SensorCard(
                          title: 'Độ ẩm đất',
                          value: '${telemetry.soilMoisture.toStringAsFixed(1)}%',
                          icon: Icons.grass,
                          color: AppColors.moisture,
                        ),
                        SensorCard(
                          title: 'Nhiệt độ',
                          value: '${telemetry.temperature.toStringAsFixed(1)}°C',
                          icon: Icons.thermostat,
                          color: AppColors.temperature,
                        ),
                        SensorCard(
                          title: 'Độ ẩm K.Khí',
                          value: '${telemetry.humidity.toStringAsFixed(1)}%',
                          icon: Icons.cloud,
                          color: AppColors.humidity,
                        ),
                        SensorCard(
                          title: 'Mực nước',
                          value: '${telemetry.waterLevel.toStringAsFixed(1)}%',
                          icon: Icons.water,
                          color: Colors.blue,
                        ),
                      ],
                    ),
                ],
              ),
            ),
          );
        }
        return const SizedBox.shrink();
      },
    );
  }
}

class SensorCard extends StatelessWidget {
  final String title;
  final String value;
  final IconData icon;
  final Color color;

  const SensorCard({
    super.key,
    required this.title,
    required this.value,
    required this.icon,
    required this.color,
  });

  @override
  Widget build(BuildContext context) {
    return Card(
      elevation: 1,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Icon(icon, color: color, size: 28),
            Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                FittedBox(
                  fit: BoxFit.scaleDown,
                  child: Text(
                    value,
                    style: Theme.of(context).textTheme.headlineSmall?.copyWith(
                      fontWeight: FontWeight.bold,
                      color: color,
                    ),
                  ),
                ),
                Text(
                  title,
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                  style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: AppColors.textSecondary,
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}
