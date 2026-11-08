import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';

DeviceConfigModel _config({required int duration, required int water}) => DeviceConfigModel(
      deviceId: '38:3E:51:6F:C7:A8',
      autoMode: true,
      soilMoistureMin: 30,
      soilMoistureMax: 70,
      airHumidityMin: 50,
      airHumidityMax: 80,
      temperatureMin: 20,
      temperatureMax: 35,
      maxWateringDuration: duration,
      waterLevelMin: water,
    );

void main() {
  test('dữ liệu cũ lưu theo giây được đổi về phút khi lưu', () {
    final json = _config(duration: 120, water: 20).toJson();
    expect(json['max_watering_duration'], 2);
  });

  test('ngưỡng nước cũ dưới 20% được đưa lên 20%', () {
    final json = _config(duration: 2, water: 0).toJson();
    expect(json['water_level_min'], 20);
  });

  test('giá trị hợp lệ giữ nguyên', () {
    final json = _config(duration: 7, water: 45).toJson();
    expect(json['max_watering_duration'], 7);
    expect(json['water_level_min'], 45);
  });

  test('luôn nằm trong khoảng backend nhận', () {
    expect(DeviceConfigModel.normalizeDurationMinutes(0), 1);
    expect(DeviceConfigModel.normalizeDurationMinutes(10), 10);
    expect(DeviceConfigModel.normalizeDurationMinutes(11), 1);
    expect(DeviceConfigModel.normalizeDurationMinutes(6000), 10);
    expect(DeviceConfigModel.normalizeWaterLevelMin(101), 100);
  });
}
