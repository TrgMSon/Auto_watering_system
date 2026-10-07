import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../data/models/device_config_model.dart';
import '../../data/models/schedule_model.dart';
import '../bloc/automation_bloc.dart';
import '../bloc/automation_event.dart';
import '../bloc/automation_state.dart';
import '../widgets/advanced_config_bottom_sheet.dart';
import '../widgets/schedule_bottom_sheet.dart';

class AutomationPage extends StatefulWidget {
  final String deviceId; // In a real app, this might come from route params or a shared selection Bloc

  const AutomationPage({super.key, this.deviceId = 'ESP_001'});

  @override
  State<AutomationPage> createState() => _AutomationPageState();
}

class _AutomationPageState extends State<AutomationPage> {
  @override
  void initState() {
    super.initState();
    context.read<AutomationBloc>().add(AutomationLoadRequested(widget.deviceId));
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Tự động hóa')),
      body: BlocBuilder<AutomationBloc, AutomationState>(
        builder: (context, state) {
          if (state is AutomationLoading) {
            return const Center(child: CircularProgressIndicator());
          }
          if (state is AutomationError) {
            return Center(child: Text('Lỗi: \${state.message}'));
          }
          if (state is AutomationLoaded) {
            final config = state.config;
            return ListView(
              padding: const EdgeInsets.all(16),
              children: [
                // Section 1: Auto Mode Thresholds
                Text('Chế độ cảm biến', style: Theme.of(context).textTheme.titleMedium),
                const SizedBox(height: 8),
                Card(
                  clipBehavior: Clip.antiAlias,
                  child: Column(
                      children: [
                        SwitchListTile(
                          contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
                          title: const Text(
                            'Bật tưới tự động',
                            style: TextStyle(fontSize: 16, fontWeight: FontWeight.w500)
                          ),
                          activeTrackColor: AppColors.primary.withAlpha(100),
                          activeThumbColor: AppColors.primary,
                          value: config.autoMode,
                          onChanged: (val) {
                            context.read<AutomationBloc>().add(
                              AutomationConfigUpdated(config.copyWith(autoMode: val)),
                            );
                          },
                        ),
                        if (config.autoMode) ...[
                          const Divider(height: 1),
                          ListTile(
                            contentPadding: const EdgeInsets.symmetric(horizontal: 16, vertical: 4),
                            leading: Container(
                              padding: const EdgeInsets.all(8),
                              decoration: BoxDecoration(
                                color: AppColors.primary.withAlpha(25), // 0.1 * 255
                                shape: BoxShape.circle,
                              ),
                              child: const Icon(Icons.tune, color: AppColors.primary, size: 20),
                            ),
                            title: const Text('Tùy chỉnh ngưỡng tự động'),
                            subtitle: Text('Đất: ${config.soilMoistureMin.toInt()}% - ${config.soilMoistureMax.toInt()}%\nKhông khí: ${config.airHumidityMin.toInt()}% - ${config.airHumidityMax.toInt()}%\nNhiệt độ: ${config.temperatureMin.toInt()}°C - ${config.temperatureMax.toInt()}°C\nThời gian bơm: ${config.maxWateringDuration}s'),
                            trailing: const Icon(Icons.chevron_right, color: Colors.grey),
                            onTap: () {
                              _showAdvancedConfigDialog(context, config);
                            },
                          ),
                          if (state.isSaving)
                            const LinearProgressIndicator(),
                        ]
                      ],
                    ),
                ),
                
                const SizedBox(height: 24),
                
                // Section 2: Schedules
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text('Lịch hẹn giờ', style: Theme.of(context).textTheme.titleMedium),
                    IconButton(
                      icon: const Icon(Icons.add_circle, color: AppColors.primary),
                      onPressed: () {
                        _showScheduleBottomSheet(context, widget.deviceId, null);
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
                        _showScheduleBottomSheet(context, widget.deviceId, schedule);
                      },
                      child: ListTile(
                        leading: const Icon(Icons.access_time, color: AppColors.primary),
                        title: Text('${schedule.startTime.substring(0, 5)} - ${schedule.endTime.substring(0, 5)}', style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 18)),
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
                                    )
                                  )
                                );
                              },
                            ),
                            IconButton(
                              icon: const Icon(Icons.delete, color: Colors.red),
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

  void _showScheduleBottomSheet(BuildContext context, String deviceId, ScheduleModel? existingSchedule) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(20))),
      builder: (bottomSheetContext) => ScheduleBottomSheet(
        deviceId: deviceId,
        existingSchedule: existingSchedule,
      ),
    );
  }

  void _showAdvancedConfigDialog(BuildContext context, DeviceConfigModel currentConfig) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(20))),
      builder: (bottomSheetContext) => AdvancedConfigBottomSheet(
        currentConfig: currentConfig,
      ),
    );
  }
}
