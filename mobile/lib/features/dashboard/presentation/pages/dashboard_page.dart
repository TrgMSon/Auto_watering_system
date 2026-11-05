import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/utils/formatters.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/empty_device_widget.dart';
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

  Future<int?> _showPumpDurationSheet(BuildContext context) {
    const options = [
      {'label': '1 phút', 'seconds': 60},
      {'label': '2 phút (mặc định)', 'seconds': 120},
      {'label': '5 phút', 'seconds': 300},
      {'label': '10 phút', 'seconds': 600},
    ];

    return showModalBottomSheet<int>(
      context: context,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(16)),
      ),
      builder: (sheetContext) {
        return SafeArea(
          child: Padding(
            padding: const EdgeInsets.symmetric(vertical: 16),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
                  child: Text(
                    'Chọn thời gian bơm',
                    style: Theme.of(sheetContext).textTheme.titleMedium,
                  ),
                ),
                ...options.map((opt) {
                  final seconds = opt['seconds'] as int;
                  final label = opt['label'] as String;
                  return ListTile(
                    title: Text(label),
                    onTap: () => Navigator.of(sheetContext).pop(seconds),
                  );
                }),
              ],
            ),
          ),
        );
      },
    );
  }

  @override
  Widget build(BuildContext context) {
    return BlocConsumer<DashboardBloc, DashboardState>(
      listenWhen: (previous, current) =>
          current is DashboardLoaded &&
          current.pumpErrorMessage != null &&
          (previous is! DashboardLoaded ||
              previous.pumpErrorMessage != current.pumpErrorMessage),
      listener: (context, state) {
        if (state is DashboardLoaded && state.pumpErrorMessage != null) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(state.pumpErrorMessage!),
              backgroundColor: Theme.of(context).colorScheme.error,
            ),
          );
        }
      },
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
              body: EmptyDeviceWidget(
                onReload: () => context
                    .read<DashboardBloc>()
                    .add(const DashboardLoadRequested()),
              ),
            );
          }
          
          final selectedDevice = state.selectedDevice ?? state.devices.first;
          final telemetry = state.telemetry;
          final pumpStatus = state.pumpStatus ?? telemetry?.isPumpOn;
          final isOnline = selectedDevice.isOnline;

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
                        state.isBleConnected
                            ? Icons.bluetooth_connected
                            : (isOnline ? Icons.wifi : Icons.wifi_off),
                        color: state.isBleConnected
                            ? Colors.blueAccent
                            : (isOnline ? Colors.greenAccent : Colors.grey),
                        size: 20,
                      ),
                      const SizedBox(width: 8),
                      Text(
                        isOnline ? 'Online' : 'Offline',
                        style: const TextStyle(fontSize: 12),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            body: RefreshIndicator(
              onRefresh: () async {
                final bloc = context.read<DashboardBloc>();
                bloc.add(const DashboardLoadRequested());
                await bloc.stream.firstWhere(
                  (state) => state is! DashboardLoaded || !state.isRefreshing,
                  orElse: () => bloc.state,
                );
              },
              child: ListView(
                padding: const EdgeInsets.all(16),
                children: [
                  if (state.errorMessage != null)
                    Padding(
                      padding: const EdgeInsets.only(bottom: 12),
                      child: Text(
                        state.errorMessage!,
                        style: TextStyle(color: Theme.of(context).colorScheme.error),
                      ),
                    ),
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
                                      if (!isOnline)
                                        Text(
                                          'Thiết bị đang offline',
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                            color: AppColors.textSecondary,
                                          ),
                                        )
                                      else if (state.isTogglingPump)
                                        Text(
                                          'Đang chờ thiết bị xác nhận...',
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                            color: AppColors.textSecondary,
                                          ),
                                        )
                                      else if (pumpStatus == true && state.pumpOffAt != null)
                                        PumpCountdownText(pumpOffAt: state.pumpOffAt!)
                                      else
                                        Text(
                                          pumpStatus == null
                                              ? 'Chưa có trạng thái từ thiết bị'
                                              : (pumpStatus
                                                  ? 'Đang tưới nước'
                                                  : 'Đang tắt'),
                                          maxLines: 1,
                                          overflow: TextOverflow.ellipsis,
                                          style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                                            color: pumpStatus == true
                                                ? AppColors.primary
                                                : AppColors.textSecondary,
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
                                  value: pumpStatus ?? false,
                                  onChanged: isOnline
                                      ? (value) async {
                                          if (!value) {
                                            context
                                                .read<DashboardBloc>()
                                                .add(const DashboardPumpToggled(false));
                                            return;
                                          }
                                          final seconds = await _showPumpDurationSheet(context);
                                          if (seconds != null && context.mounted) {
                                            context.read<DashboardBloc>().add(
                                              DashboardPumpToggled(true, durationSeconds: seconds),
                                            );
                                          }
                                        }
                                      : null,
                                ),
                        ],
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  
                  // Sensor Grid
                  Text('Thông số hiện tại', style: Theme.of(context).textTheme.titleMedium),
                  if (telemetry != null) ...[
                    const SizedBox(height: 4),
                    Text(
                      'Cập nhật lần cuối: ${Formatters.formatDateTime(telemetry.recordedAt)}',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: AppColors.textSecondary,
                      ),
                    ),
                  ],
                  const SizedBox(height: 16),
                  
                  if (state.isTelemetryEmpty)
                    const Center(
                      child: Padding(
                        padding: EdgeInsets.all(32.0),
                        child: Text('Thiết bị chưa gửi dữ liệu'),
                      ),
                    )
                  else if (telemetry == null)
                    const Center(
                      child: Padding(
                        padding: EdgeInsets.all(32.0),
                        child: CircularProgressIndicator(),
                      ),
                    )
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
                          value: '${telemetry.waterLevel.toStringAsFixed(1)} cm',
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

class PumpCountdownText extends StatefulWidget {
  final DateTime pumpOffAt;

  const PumpCountdownText({
    super.key,
    required this.pumpOffAt,
  });

  @override
  State<PumpCountdownText> createState() => _PumpCountdownTextState();
}

class _PumpCountdownTextState extends State<PumpCountdownText> {
  Timer? _timer;
  late Duration _remaining;

  @override
  void initState() {
    super.initState();
    _updateRemaining();
    _timer = Timer.periodic(const Duration(seconds: 1), (_) {
      if (mounted) {
        setState(_updateRemaining);
      }
    });
  }

  @override
  void didUpdateWidget(covariant PumpCountdownText oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.pumpOffAt != widget.pumpOffAt) {
      _updateRemaining();
    }
  }

  void _updateRemaining() {
    final diff = widget.pumpOffAt.difference(DateTime.now());
    _remaining = diff.isNegative ? Duration.zero : diff;
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_remaining == Duration.zero) {
      return Text(
        'Đang tắt',
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: Theme.of(context).textTheme.bodyMedium?.copyWith(
              color: AppColors.textSecondary,
            ),
      );
    }
    final minutes = (_remaining.inSeconds ~/ 60).toString().padLeft(2, '0');
    final seconds = (_remaining.inSeconds % 60).toString().padLeft(2, '0');
    return Text(
      'Còn $minutes:$seconds',
      maxLines: 1,
      overflow: TextOverflow.ellipsis,
      style: Theme.of(context).textTheme.bodyMedium?.copyWith(
            color: AppColors.primary,
            fontWeight: FontWeight.w500,
          ),
    );
  }
}
