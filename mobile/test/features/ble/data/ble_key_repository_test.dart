import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/ble/data/ble_key_repository.dart';

class MockApiClient extends Mock implements ApiClient {}

class MockSecureStorage extends Mock implements FlutterSecureStorage {}

void main() {
  const deviceId = '38:3E:51:6F:C7:A8';
  const apiPath = '/devices/38%3A3E%3A51%3A6F%3AC7%3AA8/ble-secret-key';
  const storageKey = 'ble_secret_key_38:3E:51:6F:C7:A8';

  late MockApiClient api;
  late MockSecureStorage storage;
  late BleKeyRepository repository;

  setUp(() {
    api = MockApiClient();
    storage = MockSecureStorage();
    repository = BleKeyRepository(api, storage: storage);

    when(() => storage.write(key: any(named: 'key'), value: any(named: 'value')))
        .thenAnswer((_) async {});
    when(() => storage.delete(key: any(named: 'key'))).thenAnswer((_) async {});
    when(() => storage.read(key: any(named: 'key'))).thenAnswer((_) async => 'khoa-cu');
  });

  void apiFails(int statusCode) {
    when(() => api.get(apiPath)).thenThrow(ApiException(statusCode, 'lỗi'));
  }

  test('lấy khóa từ server và lưu lại', () async {
    when(() => api.get(apiPath)).thenAnswer((_) async => {'ble_secret_key': 'khoa-moi'});

    expect(await repository.getKey(deviceId), 'khoa-moi');
    verify(() => storage.write(key: storageKey, value: 'khoa-moi')).called(1);
  });

  for (final status in [403, 404]) {
    test('server trả $status thì xóa khóa đã lưu', () async {
      apiFails(status);

      expect(await repository.getKey(deviceId), isNull);
      verify(() => storage.delete(key: storageKey)).called(1);
    });
  }

  for (final status in [0, 408, 503]) {
    test('không gọi được server ($status) thì dùng khóa đã lưu', () async {
      apiFails(status);

      expect(await repository.getKey(deviceId), 'khoa-cu');
      verifyNever(() => storage.delete(key: any(named: 'key')));
    });
  }

  for (final status in [401, 409]) {
    test('lỗi $status được báo lên và không xóa khóa', () async {
      apiFails(status);

      await expectLater(repository.getKey(deviceId), throwsA(isA<ApiException>()));
      verifyNever(() => storage.delete(key: any(named: 'key')));
    });
  }

  test('phản hồi không có khóa thì không ghi đè khóa cũ', () async {
    when(() => api.get(apiPath)).thenAnswer((_) async => {});

    await expectLater(repository.getKey(deviceId), throwsA(isA<ApiException>()));
    verifyNever(() => storage.write(key: any(named: 'key'), value: any(named: 'value')));
  });
}
