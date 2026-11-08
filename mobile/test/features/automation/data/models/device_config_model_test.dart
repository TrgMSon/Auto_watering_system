import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';

DeviceConfigModel _config({required int water}) => DeviceConfigModel(
      deviceId: '38:3E:51:6F:C7:A8',
      autoMode: true,
      soilMoistureMin: 30,
      soilMoistureMax: 70,
      airHumidityMin: 50,
      airHumidityMax: 80,
      temperatureMin: 20,
      temperatureMax: 35,
      waterLevelMin: water,
    );

void main() {
  test('không gửi thời gian tưới vì firmware cố định giá trị này', () {
    expect(_config(water: 20).toJson().containsKey('max_watering_duration'), isFalse);
  });

  test('ngưỡng nước cũ dưới 20% được đưa lên 20%', () {
    final json = _config(water: 0).toJson();
    expect(json['water_level_min'], 20);
  });

  test('giá trị hợp lệ giữ nguyên', () {
    final json = _config(water: 45).toJson();
    expect(json['water_level_min'], 45);
  });

  test('luôn nằm trong khoảng backend nhận', () {
    expect(DeviceConfigModel.normalizeWaterLevelMin(101), 100);
  });
}
