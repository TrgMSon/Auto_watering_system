import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/constants/api_endpoints.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/sharing/data/repositories/api_sharing_repository.dart';
import 'package:watering_app/features/sharing/domain/entities/device_share.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  const viewersPath = '/devices/AA%3ABB/viewers';

  late MockApiClient apiClient;
  late ApiSharingRepository repository;

  setUp(() {
    apiClient = MockApiClient();
    repository = ApiSharingRepository(apiClient);
  });

  test('loads viewers only for devices the user owns', () async {
    when(() => apiClient.get(ApiEndpoints.devices)).thenAnswer(
      (_) async => {
        'data': [
          {'id': 'AA:BB', 'device_name': 'Vườn', 'permission': 'owner'},
          {'id': 'CC:DD', 'device_name': 'Nhà bạn', 'permission': 'viewer'},
        ],
      },
    );
    when(() => apiClient.get(viewersPath)).thenAnswer(
      (_) async => {
        'data': [
          {'viewerId': 7, 'email': 'a@test.com', 'permission': 'viewer'},
          {'viewerId': 8, 'email': 'b@test.com', 'permission': 'viewer_locked'},
        ],
      },
    );

    final shares = await repository.getOwnedDeviceShares();

    expect(shares, const [
      DeviceShare(
        deviceId: 'AA:BB',
        deviceName: 'Vườn',
        viewers: [
          SharedViewer(id: 7, email: 'a@test.com', isLocked: false),
          SharedViewer(id: 8, email: 'b@test.com', isLocked: true),
        ],
      ),
    ]);
    verifyNever(() => apiClient.get('/devices/CC%3ADD/viewers'));
  });

  test('sends the email when adding a viewer', () async {
    when(() => apiClient.post(viewersPath, body: any(named: 'body')))
        .thenAnswer((_) async => {});

    await repository.addViewer('AA:BB', 'a@test.com');

    verify(() => apiClient.post(viewersPath, body: {'email': 'a@test.com'})).called(1);
  });

  test('uses POST to lock and DELETE to unlock', () async {
    when(() => apiClient.post('$viewersPath/7/lock')).thenAnswer((_) async => {});
    when(() => apiClient.delete('$viewersPath/7/lock')).thenAnswer((_) async {});

    await repository.setViewerLocked('AA:BB', 7, locked: true);
    await repository.setViewerLocked('AA:BB', 7, locked: false);

    verify(() => apiClient.post('$viewersPath/7/lock')).called(1);
    verify(() => apiClient.delete('$viewersPath/7/lock')).called(1);
  });

  test('deletes the viewer when removing', () async {
    when(() => apiClient.delete('$viewersPath/7')).thenAnswer((_) async {});

    await repository.removeViewer('AA:BB', 7);

    verify(() => apiClient.delete('$viewersPath/7')).called(1);
  });
}
