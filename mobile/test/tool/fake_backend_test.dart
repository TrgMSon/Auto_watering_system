import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/constants/api_endpoints.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/auth/data/repositories/api_auth_repository.dart';
import 'package:watering_app/features/auth/domain/entities/user.dart';
import 'package:watering_app/features/automation/data/models/schedule_model.dart';
import 'package:watering_app/features/automation/data/repositories/automation_repository_impl.dart';
import 'package:watering_app/features/dashboard/data/repositories/dashboard_repository_impl.dart';

import '../../tool/fake_backend.dart';

class MockSecureStorage extends Mock implements FlutterSecureStorage {}

/// Chuyển request của ApiClient sang cổng của backend giả, vì baseUrl là hằng số lúc build.
class LocalRedirectClient extends http.BaseClient {
  LocalRedirectClient(this.port);

  final int port;
  final http.Client _inner = http.Client();

  @override
  Future<http.StreamedResponse> send(http.BaseRequest request) {
    final target = request.url.replace(scheme: 'http', host: '127.0.0.1', port: port);
    final redirected = http.Request(request.method, target)
      ..headers.addAll(request.headers);
    if (request is http.Request) redirected.bodyBytes = request.bodyBytes;
    return _inner.send(redirected);
  }

  @override
  void close() => _inner.close();
}

void main() {
  late FakeBackend backend;
  late ApiClient apiClient;
  late ApiAuthRepository authRepository;
  late DashboardRepositoryImpl dashboardRepository;

  setUp(() async {
    backend = FakeBackend();
    await backend.start();

    final tokens = <String, String>{};
    final storage = MockSecureStorage();
    when(() => storage.read(key: any(named: 'key')))
        .thenAnswer((invocation) async => tokens[invocation.namedArguments[#key]]);
    when(() => storage.write(key: any(named: 'key'), value: any(named: 'value')))
        .thenAnswer((invocation) async {
      tokens[invocation.namedArguments[#key] as String] =
          invocation.namedArguments[#value] as String;
    });
    when(() => storage.delete(key: any(named: 'key'))).thenAnswer((invocation) async {
      tokens.remove(invocation.namedArguments[#key]);
    });

    apiClient = ApiClient(client: LocalRedirectClient(backend.port), storage: storage);
    authRepository = ApiAuthRepository(apiClient);
    dashboardRepository = DashboardRepositoryImpl(apiClient);
  });

  tearDown(() => backend.stop());

  Future<User> loginAsUser() =>
      authRepository.login(username: 'user@test.com', password: testPassword);

  Matcher apiExceptionWithStatus(int statusCode) => throwsA(
        isA<ApiException>().having((error) => error.statusCode, 'statusCode', statusCode),
      );

  group('auth', () {
    test('logs in and restores the stored session', () async {
      final user = await loginAsUser();

      expect(user.email, 'user@test.com');
      expect(user.role, UserRole.user);
      expect(await authRepository.getCurrentUser(), isNotNull);
    });

    test('rejects a wrong password', () {
      expect(
        authRepository.login(username: 'user@test.com', password: 'wrong-password'),
        apiExceptionWithStatus(401),
      );
    });

    test('expire makes the client refresh and retry on its own', () async {
      await loginAsUser();
      await backend.runCommand('expire');

      expect(await dashboardRepository.getDevices(), hasLength(5));
    });

    test('revoke rejects the session', () async {
      await loginAsUser();
      await backend.runCommand('revoke');

      await expectLater(dashboardRepository.getDevices(), apiExceptionWithStatus(401));
    });

    test('blocks admin accounts from device APIs like the real backend', () async {
      await authRepository.login(username: 'admin@test.com', password: testPassword);

      await expectLater(dashboardRepository.getDevices(), apiExceptionWithStatus(403));
    });
  });

  group('devices', () {
    test('lists scenario devices and follows offline and online commands', () async {
      await loginAsUser();

      final devices = await dashboardRepository.getDevices();
      expect(devices.map((device) => device.status), [
        'active',
        'offline',
        'active',
        'active',
        'active',
      ]);
      expect(devices.map((device) => device.isOnline).toList(), [true, false, true, true, true]);

      await backend.runCommand('offline 1');
      await backend.runCommand('online 2');
      final updated = await dashboardRepository.getDevices();
      expect(updated[0].status, 'offline');
      expect(updated[1].status, 'active');
    });

    test('returns no devices for the empty account', () async {
      await authRepository.login(username: 'empty@test.com', password: testPassword);

      expect(await dashboardRepository.getDevices(), isEmpty);
    });

    test('down and up simulate losing and regaining the server', () async {
      await loginAsUser();

      await backend.runCommand('down');
      await expectLater(dashboardRepository.getDevices(), apiExceptionWithStatus(0));

      await backend.runCommand('up');
      expect(await dashboardRepository.getDevices(), hasLength(5));
    });
  });

  group('telemetry', () {
    test('returns the latest reading, or null when a device has no data', () async {
      await loginAsUser();

      final reading = await dashboardRepository.watchTelemetry('ESP_01', null).first;
      expect(reading, isNotNull);
      expect(reading!.deviceId, 'ESP_01');
      expect(reading.soilMoisture, inInclusiveRange(35, 75));

      expect(await dashboardRepository.watchTelemetry('ESP_04', null).first, isNull);
    });

    test('returns 24 hours of history in time order', () async {
      await loginAsUser();
      final now = DateTime.now();

      final response = await apiClient.get(
        '${ApiEndpoints.devices}/ESP_01/telemetry',
        queryParameters: {
          'from': now.subtract(const Duration(hours: 24)).toIso8601String(),
          'to': now.toIso8601String(),
        },
      );

      final rows = (response['data'] as List).cast<Map<String, dynamic>>();
      expect(rows.length, inInclusiveRange(96, 97));
      final times = rows.map((row) => DateTime.parse(row['recorded_at'] as String)).toList();
      expect(times, orderedEquals([...times]..sort()));
    });
  });

  group('pump', () {
    test('ESP_01 confirms the command', () async {
      await loginAsUser();

      await expectLater(dashboardRepository.togglePump('ESP_01', true, duration: 60), completes);
    });

    test('ESP_03 rejects the command with a message', () async {
      await loginAsUser();

      await expectLater(
        dashboardRepository.togglePump('ESP_03', true),
        throwsA(isA<ApiException>()
            .having((error) => error.message, 'message', 'Thiết bị từ chối lệnh bơm.')),
      );
    });

    test('ESP_05 never answers, so the app times out', () async {
      await loginAsUser();

      await expectLater(dashboardRepository.togglePump('ESP_05', true), apiExceptionWithStatus(408));
    });
  });

  group('automation', () {
    late AutomationRepositoryImpl repository;

    setUp(() async {
      await loginAsUser();
      repository = AutomationRepositoryImpl(apiClient);
    });

    test('saves device config', () async {
      final config = await repository.getDeviceConfig('ESP_01');
      await repository.updateDeviceConfig(config.copyWith(autoMode: true, soilMoistureMin: 45));

      final saved = await repository.getDeviceConfig('ESP_01');
      expect(saved.deviceId, 'ESP_01');
      expect(saved.autoMode, isTrue);
      expect(saved.soilMoistureMin, 45);
    });

    test('creates, updates and deletes a schedule', () async {
      await repository.addSchedule(const ScheduleModel(
        deviceId: 'ESP_01',
        startTime: '06:00:00',
        endTime: '06:15:00',
        daysOfWeek: 'MON,WED',
        isEnabled: true,
      ));
      final created = (await repository.getSchedules('ESP_01')).single;
      expect(created.id, isNotNull);

      await repository.updateSchedule(ScheduleModel(
        id: created.id,
        deviceId: 'ESP_01',
        startTime: '07:00:00',
        endTime: '07:15:00',
        daysOfWeek: 'FRI',
        isEnabled: false,
      ));
      final updated = (await repository.getSchedules('ESP_01')).single;
      expect(updated.startTime, '07:00:00');
      expect(updated.isEnabled, isFalse);

      await repository.deleteSchedule('ESP_01', created.id!);
      expect(await repository.getSchedules('ESP_01'), isEmpty);
    });
  });
}
