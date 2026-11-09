import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/constants/api_endpoints.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/device_management/data/repositories/api_device_repository.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  test('maps the backend active status to online and others to offline', () async {
    final apiClient = MockApiClient();
    when(() => apiClient.get(ApiEndpoints.devices)).thenAnswer(
      (_) async => {
        'data': [
          {'id': 'A', 'device_name': 'Vườn 1', 'status': 'active'},
          {'id': 'B', 'device_name': 'Vườn 2', 'status': 'offline'},
          {'id': 'C', 'device_name': 'Vườn 3', 'status': 'provisioned'},
        ],
      },
    );

    final devices = await ApiDeviceRepository(apiClient).getDevices();

    expect(devices.map((device) => device.isOnline).toList(), [true, false, false]);
  });
}
