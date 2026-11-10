import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../ble/presentation/bloc/ble_session_cubit.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';
import '../bloc/automation_bloc.dart';
import '../bloc/automation_event.dart';
import '../bloc/automation_state.dart';
import '../widgets/advanced_config_bottom_sheet.dart';
import '../widgets/schedule_bottom_sheet.dart';

class AutomationPage extends StatefulWidget {
  final String deviceId;

  /// Firmware chỉ nhận cấu hình mới khi bơm đang tắt.
  final bool isPumpOn;
  final bool isDeviceOnline;

  const AutomationPage(
      {super.key,
      this.deviceId = '',
      this.isPumpOn = false,
      this.isDeviceOnline = false});

  @override
  State<AutomationPage> createState() => _AutomationPageState();
}

class _AutomationPageState extends State<AutomationPage> {
  @override
  void initState() {
    super.initState();
    if (widget.deviceId.isNotEmpty) {
      context.read<AutomationBloc>().add(AutomationLoadRequested(
          widget.deviceId,
          loadFromBackend: widget.isDeviceOnline));
      if (!widget.isDeviceOnline) {
        // In offline mode load the current AUTO/MANUAL state directly from ESP32.
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (mounted) _connectBle(context);
        });
      }
    }
  }

  @override
  void didUpdateWidget(covariant AutomationPage oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (widget.deviceId.isNotEmpty && widget.deviceId != oldWidget.deviceId) {
      context.read<AutomationBloc>().add(AutomationLoadRequested(
          widget.deviceId,
          loadFromBackend: widget.isDeviceOnline));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Tự động hóa'),
        actions: [
          IconButton(
            tooltip: 'Kết nối Bluetooth với thiết bị',
            icon: const Icon(Icons.bluetooth),
            onPressed:
                widget.deviceId.isEmpty ? null : () => _connectBle(context),
          ),
        ],
      ),
      body: BlocBuilder<AutomationBloc, AutomationState>(
        builder: (context, state) {
          if (state is AutomationLoading) {
            return const AppLoadingIndicator(
              message: 'Đang tải cấu hình tự động...',
            );
          }
          if (state is AutomationError) {
            return AppErrorWidget(
              message: state.message,
              onRetry: () {
                if (widget.deviceId.isNotEmpty) {
                  context.read<AutomationBloc>().add(AutomationLoadRequested(
                      widget.deviceId,
                      loadFromBackend: widget.isDeviceOnline));
                }
              },
            );
          }
          if (state is AutomationInitial) {
            return const Center(
                child: Text('Chưa tìm thấy thiết bị của tài khoản.'));
          }
          if (state is AutomationLoaded) {
            final config = state.config;
            final canPersistToBackend =
                widget.isDeviceOnline && state.errorMessage == null;
            final bleState = context.watch<BleSessionCubit>().state;
            final bleConnected = bleState.isConnectedTo(widget.deviceId);
            final bleMode = bleState.sample?.isAutoMode;
            return ListView(
              padding: const EdgeInsets.all(16),
              children: [
                if (state.errorMessage != null)
                  Padding(
                    padding: const EdgeInsets.only(bottom: 12),
                    child: Text(
                      state.errorMessage!,
                      style:
                          TextStyle(color: Theme.of(context).colorScheme.error),
                    ),
                  ),
                // Section 1: Auto Mode Thresholds
                Text('Chế độ cảm biến',
                    style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 8),
                Card(
                  clipBehavior: Clip.antiAlias,
                  child: Column(
                    children: [
                      SwitchListTile(
                        contentPadding: const EdgeInsets.symmetric(
                            horizontal: 16, vertical: 4),
                        title: const Text('Bật tưới tự động',
                            style: TextStyle(
                                fontSize: 16, fontWeight: FontWeight.w500)),
                        activeTrackColor: AppColors.primary.withAlpha(100),
                        activeThumbColor: AppColors.primary,
                        value: bleConnected
                            ? (bleMode ?? config.autoMode)
                            : config.autoMode,
                        onChanged: state.isSaving ||
                                (!widget.isDeviceOnline && !bleConnected)
                            ? null
                            : (val) => _setAutoMode(
                                context, config, val, canPersistToBackend),
                      ),
                      const Divider(height: 1),
                      ListTile(
                        enabled: widget.isDeviceOnline &&
                            _configLockReason(config) == null,
                        contentPadding: const EdgeInsets.symmetric(
                            horizontal: 16, vertical: 4),
                        leading: Container(
                          padding: const EdgeInsets.all(8),
                          decoration: BoxDecoration(
                            color: AppColors.primary.withAlpha(25), // 0.1 * 255
                            shape: BoxShape.circle,
                          ),
                          child: const Icon(Icons.tune,
                              color: AppColors.primary, size: 20),
                        ),
                        title: const Text('Tùy chỉnh ngưỡng tự động'),
                        subtitle: Text([
                          'Đất: ${config.soilMoistureMin.toInt()}% - ${config.soilMoistureMax.toInt()}%',
                          'Không khí: ${config.airHumidityMin.toInt()}% - ${config.airHumidityMax.toInt()}%',
                          'Nhiệt độ: ${config.temperatureMin.toInt()}°C - ${config.temperatureMax.toInt()}°C',
                          'Nước tối thiểu: ${config.waterLevelMin}%',
                          if (_configLockReason(config) case final reason?)
                            reason,
                        ].join('\n')),
                        trailing:
                            const Icon(Icons.chevron_right, color: Colors.grey),
                        onTap: () {
                          _showAdvancedConfigDialog(context, config);
                        },
                      ),
                      if (state.isSaving) const LinearProgressIndicator(),
                    ],
                  ),
                ),

                const SizedBox(height: 24),

                // Section 2: Schedules
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text('Lịch hẹn giờ',
                        style: Theme.of(context).textTheme.titleMedium),
                    IconButton(
                      icon: const Icon(Icons.add_circle,
                          color: AppColors.primary),
                      onPressed: () {
                        _showScheduleBottomSheet(
                            context, widget.deviceId, null);
                      },
                    ),
                  ],
                ),
                const SizedBox(height: 8),
                if (state.schedules.isEmpty)
                  const Padding(
                    padding: EdgeInsets.all(16.0),
                    child: Center(child: Text('Chưa có lịch hẹn giờ nào.')),
                  )
                else
                  ...state.schedules.map((schedule) => Card(
                        clipBehavior: Clip.antiAlias,
                        child: InkWell(
                          onTap: () {
                            _showScheduleBottomSheet(
                                context, widget.deviceId, schedule);
                          },
                          child: ListTile(
                            leading: const Icon(Icons.access_time,
                                color: AppColors.primary),
                            title: Text(
                                '${schedule.startTime.substring(0, 5)} - ${schedule.endTime.substring(0, 5)}',
                                style: const TextStyle(
                                    fontWeight: FontWeight.bold, fontSize: 18)),
                            subtitle: Text(schedule.daysOfWeek),
                            trailing: Row(
                              mainAxisSize: MainAxisSize.min,
                              children: [
                                Switch(
                                  value: schedule.isEnabled,
                                  onChanged: (val) {
                                    context.read<AutomationBloc>().add(
                                            AutomationScheduleUpdated(
                                                ScheduleModel(
                                          id: schedule.id,
                                          deviceId: schedule.deviceId,
                                          startTime: schedule.startTime,
                                          endTime: schedule.endTime,
                                          daysOfWeek: schedule.daysOfWeek,
                                          isEnabled: val,
                                          durationMinutes:
                                              schedule.durationMinutes,
                                        )));
                                  },
                                ),
                                IconButton(
                                  icon: const Icon(Icons.delete,
                                      color: Colors.red),
                                  onPressed: () {
                                    context.read<AutomationBloc>().add(
                                          AutomationScheduleDeleted(
                                            scheduleId: schedule.id!,
                                            deviceId: widget.deviceId,
                                          ),
                                        );
                                  },
                                ),
                              ],
                            ),
                          ),
                        ),
                      )),
              ],
            );
          }
          return const SizedBox.shrink();
        },
      ),
    );
  }

  Future<void> _connectBle(BuildContext context) async {
    final cubit = context.read<BleSessionCubit>();
    await cubit.connect(widget.deviceId);
    if (!context.mounted || cubit.state.status != BleSessionStatus.connected) {
      if (context.mounted) {
        _showError(
            context, cubit.state.errorMessage ?? 'Không kết nối được BLE.');
      }
    }
  }

  Future<void> _setAutoMode(BuildContext context, DeviceConfigModel config,
      bool enabled, bool canPersistToBackend) async {
    final cubit = context.read<BleSessionCubit>();
    if (!cubit.state.isConnectedTo(widget.deviceId)) {
      await cubit.connect(widget.deviceId);
    }
    if (!context.mounted || !cubit.state.isConnectedTo(widget.deviceId)) {
      if (context.mounted) {
        _showError(
            context,
            cubit.state.errorMessage ??
                'Hãy kết nối Bluetooth để điều khiển thiết bị.');
      }
      return;
    }
    final error = await cubit.setAutoMode(enabled);
    if (!context.mounted) return;
    if (error != null) {
      _showError(context, error);
      return;
    }
    final updated = config.copyWith(autoMode: enabled);
    // BLE đã áp dụng lệnh tại thiết bị; chỉ đồng bộ backend khi cloud online.
    context
        .read<AutomationBloc>()
        .add(AutomationConfigUpdated(updated, persist: canPersistToBackend));
  }

  void _showError(BuildContext context, String message) {
    ScaffoldMessenger.of(context)
        .showSnackBar(SnackBar(content: Text(message)));
  }

  /// Firmware chỉ nhận ngưỡng mới khi đang ở chế độ thủ công và bơm tắt.
  String? _configLockReason(DeviceConfigModel config) {
    if (config.autoMode) return 'Tắt tưới tự động để sửa ngưỡng.';
    if (widget.isPumpOn) return 'Chờ bơm tắt để sửa ngưỡng.';
    return null;
  }

  void _showScheduleBottomSheet(
      BuildContext context, String deviceId, ScheduleModel? existingSchedule) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(20))),
      builder: (bottomSheetContext) => ScheduleBottomSheet(
        deviceId: deviceId,
        existingSchedule: existingSchedule,
      ),
    );
  }

  void _showAdvancedConfigDialog(
      BuildContext context, DeviceConfigModel currentConfig) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(
          borderRadius: BorderRadius.vertical(top: Radius.circular(20))),
      builder: (bottomSheetContext) => AdvancedConfigBottomSheet(
        currentConfig: currentConfig,
      ),
    );
  }
}
