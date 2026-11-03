import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/dashboard/data/models/device_model.dart';

void main() {
  DeviceModel deviceWithStatus(String status) => DeviceModel(
        id: 'AA:BB:CC:DD:EE:FF',
        deviceName: 'Vườn',
        status: status,
        permission: 'owner',
      );

  group('DeviceModel.isOnline', () {
    test('is true only for the active status from the backend', () {
      expect(deviceWithStatus('active').isOnline, isTrue);
      expect(deviceWithStatus('ACTIVE').isOnline, isTrue);
    });

    test('is false for every other backend status', () {
      const statuses = [
        'registered',
        'provisioned',
        'offline',
        'fault',
        'maintenance',
        'decommissioned',
      ];
      for (final status in statuses) {
        expect(deviceWithStatus(status).isOnline, isFalse, reason: status);
      }
    });

    test('treats a missing status as offline', () {
      final device = DeviceModel.fromJson(const {'id': 'x', 'device_name': 'Vườn'});

      expect(device.isOnline, isFalse);
    });
  });
}
