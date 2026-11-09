import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../data/models/device_config_model.dart';
import '../bloc/automation_bloc.dart';
import '../bloc/automation_event.dart';

class AdvancedConfigBottomSheet extends StatefulWidget {
  final DeviceConfigModel currentConfig;

  const AdvancedConfigBottomSheet({super.key, required this.currentConfig});

  @override
  State<AdvancedConfigBottomSheet> createState() => _AdvancedConfigBottomSheetState();
}

class _AdvancedConfigBottomSheetState extends State<AdvancedConfigBottomSheet> {
  late double airHumidityMin;
  late double airHumidityMax;
  late double soilMoistureMin;
  late double soilMoistureMax;
  late double tempMin;
  late double tempMax;
  late double waterMin;

  // Firmware chỉ nhận ngưỡng nước từ 20% đến 100% bồn.
  static const double _waterMinLowest = 20;
  static const double _waterMinHighest = 100;

  @override
  void initState() {
    super.initState();
    _initValues();
  }

  void _initValues() {
    airHumidityMin = widget.currentConfig.airHumidityMin;
    airHumidityMax = widget.currentConfig.airHumidityMax;
    soilMoistureMin = widget.currentConfig.soilMoistureMin;
    soilMoistureMax = widget.currentConfig.soilMoistureMax;
    tempMin = widget.currentConfig.temperatureMin;
    tempMax = widget.currentConfig.temperatureMax;
    // Dữ liệu cũ có thể nằm ngoài khoảng firmware nhận và làm hỏng Slider.
    waterMin = widget.currentConfig.waterLevelMin
        .toDouble()
        .clamp(_waterMinLowest, _waterMinHighest);
  }

  void _resetToDefaults() {
    setState(() {
      airHumidityMin = 50;
      airHumidityMax = 80;
      soilMoistureMin = 40;
      soilMoistureMax = 80;
      tempMin = 20;
      tempMax = 35;
      waterMin = _waterMinLowest;
    });
  }

  void _saveConfig() {
    context.read<AutomationBloc>().add(
      AutomationConfigUpdated(widget.currentConfig.copyWith(
        airHumidityMin: airHumidityMin,
        airHumidityMax: airHumidityMax,
        soilMoistureMin: soilMoistureMin,
        soilMoistureMax: soilMoistureMax,
        temperatureMin: tempMin,
        temperatureMax: tempMax,
        waterLevelMin: waterMin.round(),
      ))
    );
    Navigator.pop(context);
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
        top: 24, left: 24, right: 24,
      ),
      child: SingleChildScrollView(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                const Text('Cài đặt ngưỡng tưới', style: TextStyle(fontSize: 20, fontWeight: FontWeight.bold)),
                TextButton(
                  onPressed: _resetToDefaults,
                  child: const Text('Mặc định', style: TextStyle(color: AppColors.primary)),
                )
              ],
            ),
            const SizedBox(height: 8),
            
            // Air Humidity
            Text('Độ ẩm không khí: ${airHumidityMin.toInt()}% - ${airHumidityMax.toInt()}%', style: const TextStyle(fontWeight: FontWeight.bold)),
            RangeSlider(
              values: RangeValues(airHumidityMin, airHumidityMax),
              min: 0,
              max: 100,
              divisions: 20,
              labels: RangeLabels('${airHumidityMin.toInt()}%', '${airHumidityMax.toInt()}%'),
              activeColor: AppColors.primary,
              onChanged: (RangeValues values) {
                setState(() {
                  airHumidityMin = values.start;
                  airHumidityMax = values.end;
                });
              },
            ),
            const SizedBox(height: 8),

            // Soil Moisture
            Text('Độ ẩm đất: ${soilMoistureMin.toInt()}% - ${soilMoistureMax.toInt()}%', style: const TextStyle(fontWeight: FontWeight.bold)),
            RangeSlider(
              values: RangeValues(soilMoistureMin, soilMoistureMax),
              min: 0,
              max: 100,
              divisions: 20,
              labels: RangeLabels('${soilMoistureMin.toInt()}%', '${soilMoistureMax.toInt()}%'),
              activeColor: AppColors.primary,
              onChanged: (RangeValues values) {
                // Firmware không nhận ngưỡng bật bằng ngưỡng tắt.
                if (values.start >= values.end) return;
                setState(() {
                  soilMoistureMin = values.start;
                  soilMoistureMax = values.end;
                });
              },
            ),
            const SizedBox(height: 8),

            // Temperature
            Text('Nhiệt độ: ${tempMin.toInt()}°C - ${tempMax.toInt()}°C', style: const TextStyle(fontWeight: FontWeight.bold)),
            RangeSlider(
              values: RangeValues(tempMin, tempMax),
              min: 0,
              max: 50,
              divisions: 50,
              labels: RangeLabels('${tempMin.toInt()}°C', '${tempMax.toInt()}°C'),
              activeColor: AppColors.primary,
              onChanged: (RangeValues values) {
                setState(() {
                  tempMin = values.start;
                  tempMax = values.end;
                });
              },
            ),
            const Text(
              'Hệ thống sẽ tổng hợp các ngưỡng trên để ra quyết định bơm tối ưu nhất.',
              style: TextStyle(color: Colors.grey, fontSize: 12),
            ),
            const SizedBox(height: 24),
            
            Text('Mực nước tối thiểu: ${waterMin.toInt()}%', style: const TextStyle(fontWeight: FontWeight.bold)),
            Slider(
              value: waterMin,
              min: _waterMinLowest,
              max: _waterMinHighest,
              divisions: 16,
              activeColor: AppColors.primary,
              onChanged: (val) {
                setState(() {
                  waterMin = val;
                });
              },
            ),
            const Text(
              'Tính năng an toàn: Không bơm khi nước trong bồn thấp hơn mức này để tránh chạy khô.',
              style: TextStyle(color: Colors.grey, fontSize: 12),
            ),
            
            const SizedBox(height: 32),
            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                style: ElevatedButton.styleFrom(backgroundColor: AppColors.primary, foregroundColor: Colors.white),
                onPressed: _saveConfig,
                child: const Text('Lưu cài đặt'),
              ),
            )
          ],
        ),
      ),
    );
  }
}
