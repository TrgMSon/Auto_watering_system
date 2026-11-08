import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/automation/data/repositories/automation_repository_impl.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  const deviceId = 'AA:BB:CC:DD:EE:FF';
  const configPath = '/devices/AA%3ABB%3ACC%3ADD%3AEE%3AFF/config';

  late MockApiClient apiClient;
  late AutomationRepositoryImpl repository;

  setUp(() {
    apiClient = MockApiClient();
    repository = AutomationRepositoryImpl(apiClient);
  });

  group('getDeviceConfig', () {
    test('returns editable defaults when the device has no config yet', () async {
      when(() => apiClient.get(configPath)).thenThrow(
        const ApiException(404, 'Chưa có cấu hình cho thiết bị này'),
      );

      final config = await repository.getDeviceConfig(deviceId);

      expect(config.deviceId, deviceId);
      expect(config.autoMode, isFalse);
      expect(config.soilMoistureMin, 40);
      expect(config.soilMoistureMax, 80);
    });

    test('still reports other API errors', () {
      when(() => apiClient.get(configPath)).thenThrow(
        const ApiException(500, 'Đã xảy ra lỗi hệ thống.'),
      );

      expect(repository.getDeviceConfig(deviceId), throwsA(isA<ApiException>()));
    });

    test('reads the saved config from the backend', () async {
      when(() => apiClient.get(configPath)).thenAnswer(
        (_) async => {
          'device_id': deviceId,
          'auto_mode': true,
          'soil_moisture_min': 30.0,
          'soil_moisture_max': 70.0,
        },
      );

      final config = await repository.getDeviceConfig(deviceId);

      expect(config.autoMode, isTrue);
      expect(config.soilMoistureMin, 30);
    });
  });
}
