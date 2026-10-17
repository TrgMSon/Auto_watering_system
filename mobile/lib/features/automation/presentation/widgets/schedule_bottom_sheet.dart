import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../data/models/schedule_model.dart';
import '../bloc/automation_bloc.dart';
import '../bloc/automation_event.dart';

class ScheduleBottomSheet extends StatefulWidget {
  final String deviceId;
  final ScheduleModel? existingSchedule;

  const ScheduleBottomSheet({super.key, required this.deviceId, this.existingSchedule});

  @override
  State<ScheduleBottomSheet> createState() => _ScheduleBottomSheetState();
}

class _ScheduleBottomSheetState extends State<ScheduleBottomSheet> {
  late TimeOfDay startTime;
  late TimeOfDay endTime;
  late List<String> selectedDays;

  final List<String> allDays = ['MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT', 'SUN'];
  final Map<String, String> dayLabels = {'MON': 'T2', 'TUE': 'T3', 'WED': 'T4', 'THU': 'T5', 'FRI': 'T6', 'SAT': 'T7', 'SUN': 'CN'};

  @override
  void initState() {
    super.initState();
    _initValues();
  }

  void _initValues() {
    final existingSchedule = widget.existingSchedule;
    startTime = existingSchedule != null 
      ? TimeOfDay(hour: int.parse(existingSchedule.startTime.split(':')[0]), minute: int.parse(existingSchedule.startTime.split(':')[1]))
      : TimeOfDay.now();
      
    endTime = existingSchedule != null 
      ? TimeOfDay(hour: int.parse(existingSchedule.endTime.split(':')[0]), minute: int.parse(existingSchedule.endTime.split(':')[1]))
      : TimeOfDay.now().replacing(minute: TimeOfDay.now().minute + 10);
      
    selectedDays = existingSchedule?.daysOfWeek
        .split(',')
        .map((e) => e.trim())
        .where((e) => e.isNotEmpty)
        .toList() ?? List.from(allDays);
  }

  void _saveSchedule() {
    if (selectedDays.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(content: Text('Vui lòng chọn ít nhất 1 ngày!')));
      return;
    }

    final String startH = startTime.hour.toString().padLeft(2, '0');
    final String startM = startTime.minute.toString().padLeft(2, '0');
    final String endH = endTime.hour.toString().padLeft(2, '0');
    final String endM = endTime.minute.toString().padLeft(2, '0');
    
    selectedDays.sort((a, b) => allDays.indexOf(a).compareTo(allDays.indexOf(b)));
    
    final updatedSchedule = ScheduleModel(
      id: widget.existingSchedule?.id,
      deviceId: widget.deviceId,
      startTime: '$startH:$startM:00',
      endTime: '$endH:$endM:00',
      daysOfWeek: selectedDays.join(','),
      isEnabled: widget.existingSchedule?.isEnabled ?? true,
    );

    if (widget.existingSchedule == null) {
      context.read<AutomationBloc>().add(AutomationScheduleAdded(updatedSchedule));
    } else {
      context.read<AutomationBloc>().add(AutomationScheduleUpdated(updatedSchedule));
    }
    
    Navigator.pop(context);
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
        top: 24, left: 24, right: 24,
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(widget.existingSchedule == null ? 'Thêm lịch hẹn giờ' : 'Sửa lịch hẹn giờ', style: const TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
          const SizedBox(height: 16),
          ListTile(
            title: const Text('Thời gian bắt đầu'),
            trailing: Text(startTime.format(context), style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: AppColors.primary)),
            onTap: () async {
              final TimeOfDay? time = await showTimePicker(context: context, initialTime: startTime);
              if (time != null) setState(() => startTime = time);
            },
          ),
          const Divider(height: 1),
          ListTile(
            title: const Text('Thời gian kết thúc'),
            trailing: Text(endTime.format(context), style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold, color: AppColors.primary)),
            onTap: () async {
              final TimeOfDay? time = await showTimePicker(context: context, initialTime: endTime);
              if (time != null) setState(() => endTime = time);
            },
          ),
          const SizedBox(height: 16),
          const Text('Lặp lại vào các ngày:', style: TextStyle(fontWeight: FontWeight.bold)),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8.0,
            runSpacing: 4.0,
            children: allDays.map((day) {
              final isSelected = selectedDays.contains(day);
              return FilterChip(
                label: Text(dayLabels[day]!),
                selected: isSelected,
                selectedColor: AppColors.primary.withAlpha(51), // 0.2 * 255 = 51
                checkmarkColor: AppColors.primary,
                onSelected: (bool selected) {
                  setState(() {
                    if (selected) {
                      selectedDays.add(day);
                    } else {
                      selectedDays.remove(day);
                    }
                  });
                },
              );
            }).toList(),
          ),
          const SizedBox(height: 24),
          SizedBox(
            width: double.infinity,
            child: ElevatedButton(
              style: ElevatedButton.styleFrom(backgroundColor: AppColors.primary, foregroundColor: Colors.white),
              onPressed: _saveSchedule,
              child: Text(widget.existingSchedule == null ? 'Lưu lịch hẹn' : 'Cập nhật lịch'),
            ),
          )
        ],
      ),
    );
  }
}
