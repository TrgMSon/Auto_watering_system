import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/dashboard/data/repositories/dashboard_repository_impl.dart';

class MockApiClient extends Mock implements ApiClient {}

void main() {
  const configPath = '/devices/ESP_001/config';

  late MockApiClient api;
  late DashboardRepositoryImpl repository;

  setUp(() {
    api = MockApiClient();
    repository = DashboardRepositoryImpl(api);
  });

  test('đọc auto_mode từ cấu hình', () async {
    when(() => api.get(configPath)).thenAnswer((_) async => {'auto_mode': true});
    expect(await repository.getAutoMode('ESP_001'), isTrue);

    when(() => api.get(configPath)).thenAnswer((_) async => {'auto_mode': false});
    expect(await repository.getAutoMode('ESP_001'), isFalse);
  });

  test('thiết bị chưa có cấu hình thì chưa bật tự động', () async {
    when(() => api.get(configPath)).thenThrow(const ApiException(404, 'không có'));
    expect(await repository.getAutoMode('ESP_001'), isFalse);
  });

  test('lỗi khác được báo lên', () async {
    when(() => api.get(configPath)).thenThrow(const ApiException(500, 'lỗi'));
    await expectLater(repository.getAutoMode('ESP_001'), throwsA(isA<ApiException>()));
  });
}
