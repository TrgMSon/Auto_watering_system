import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:http/http.dart' as http;
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/constants/api_endpoints.dart';
import 'package:watering_app/core/network/api_client.dart';

class MockHttpClient extends Mock implements http.Client {}

class MockSecureStorage extends Mock implements FlutterSecureStorage {}

class FakeBaseRequest extends Fake implements http.BaseRequest {}

void main() {
  setUpAll(() {
    registerFallbackValue(FakeBaseRequest());
  });

  late MockHttpClient mockHttpClient;
  late MockSecureStorage mockSecureStorage;
  late ApiClient apiClient;

  setUp(() {
    mockHttpClient = MockHttpClient();
    mockSecureStorage = MockSecureStorage();
    apiClient = ApiClient(
      client: mockHttpClient,
      storage: mockSecureStorage,
    );
  });

  group('ApiClient Safety Net Tests', () {
    test('get() sends GET request with Authorization header and parses response', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => 'fake_access_token');

      final fakeResponseBody = jsonEncode({
        'success': true,
        'message': 'OK',
        'data': {'id': '123', 'name': 'Device 1'},
      });

      when(() => mockHttpClient.send(any())).thenAnswer((invocation) async {
        final request = invocation.positionalArguments.first as http.Request;
        expect(request.method, equals('GET'));
        expect(request.headers['Authorization'], equals('Bearer fake_access_token'));
        expect(request.headers['Accept'], equals('application/json'));

        return http.StreamedResponse(
          Stream.value(utf8.encode(fakeResponseBody)),
          200,
          headers: {'content-type': 'application/json; charset=utf-8'},
        );
      });

      final result = await apiClient.get('/test');

      expect(result['id'], equals('123'));
      expect(result['name'], equals('Device 1'));
    });

    test('post() sends POST request with json body and returns payload', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);

      final fakeResponseBody = jsonEncode({
        'success': true,
        'data': {'status': 'created'},
      });

      when(() => mockHttpClient.send(any())).thenAnswer((invocation) async {
        final request = invocation.positionalArguments.first as http.Request;
        expect(request.method, equals('POST'));
        expect(request.body, equals(jsonEncode({'name': 'New Device'})));
        expect(request.headers['Content-Type'], equals('application/json'));

        return http.StreamedResponse(
          Stream.value(utf8.encode(fakeResponseBody)),
          201,
          headers: {'content-type': 'application/json; charset=utf-8'},
        );
      });

      final result = await apiClient.post('/devices', body: {'name': 'New Device'});
      expect(result['status'], equals('created'));
    });

    test('put() and delete() call proper methods', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);

      when(() => mockHttpClient.send(any())).thenAnswer((invocation) async {
        final request = invocation.positionalArguments.first as http.Request;
        expect(request.method, anyOf('PUT', 'DELETE'));
        return http.StreamedResponse(
          Stream.value(utf8.encode('{"success": true, "data": {}}')),
          200,
          headers: {'content-type': 'application/json'},
        );
      });

      await apiClient.put('/devices/1', body: {'state': 'on'});
      await apiClient.delete('/devices/1');
      verify(() => mockHttpClient.send(any())).called(2);
    });

    test('throws ApiException when API returns success == false or 400+', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);

      final fakeResponseBody = jsonEncode({
        'success': false,
        'message': 'Thiết bị không tồn tại.',
      });

      when(() => mockHttpClient.send(any())).thenAnswer((_) async {
        return http.StreamedResponse(
          Stream.value(utf8.encode(fakeResponseBody)),
          404,
          headers: {'content-type': 'application/json; charset=utf-8'},
        );
      });

      expect(
        () => apiClient.get('/devices/999'),
        throwsA(
          isA<ApiException>()
              .having((e) => e.statusCode, 'statusCode', 404)
              .having((e) => e.message, 'message', 'Thiết bị không tồn tại.'),
        ),
      );
    });

    test('automatically refreshes token on 401 and retries original request', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => 'old_access_token');
      when(() => mockSecureStorage.read(key: 'refresh_token'))
          .thenAnswer((_) async => 'old_refresh_token');
      when(() => mockSecureStorage.write(key: any(named: 'key'), value: any(named: 'value')))
          .thenAnswer((_) async {});

      var callCount = 0;
      when(() => mockHttpClient.send(any())).thenAnswer((invocation) async {
        final request = invocation.positionalArguments.first as http.Request;
        callCount++;

        if (callCount == 1) {
          return http.StreamedResponse(
            Stream.value(utf8.encode('{"message": "Unauthorized"}')),
            401,
          );
        } else if (callCount == 2) {
          expect(request.url.path, contains(ApiEndpoints.refreshToken));
          final refreshBody = jsonEncode({
            'success': true,
            'data': {
              'accessToken': 'new_access_token',
              'refreshToken': 'new_refresh_token',
            },
          });
          return http.StreamedResponse(
            Stream.value(utf8.encode(refreshBody)),
            200,
          );
        } else {
          expect(request.url.path, contains('/data'));
          final successBody = jsonEncode({
            'success': true,
            'data': {'result': 'success_after_refresh'},
          });
          return http.StreamedResponse(
            Stream.value(utf8.encode(successBody)),
            200,
          );
        }
      });

      final result = await apiClient.get('/data');
      expect(result['result'], equals('success_after_refresh'));
      expect(callCount, equals(3));
      verify(() => mockSecureStorage.write(key: 'access_token', value: 'new_access_token')).called(1);
      verify(() => mockSecureStorage.write(key: 'refresh_token', value: 'new_refresh_token')).called(1);
    });

    test('concurrency: multiple simultaneous 401 requests trigger only ONE refresh call', () async {
      String currentAccessToken = 'old_token';
      String currentRefreshToken = 'valid_refresh';

      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => currentAccessToken);
      when(() => mockSecureStorage.read(key: 'refresh_token'))
          .thenAnswer((_) async => currentRefreshToken);
      when(() => mockSecureStorage.write(key: 'access_token', value: any(named: 'value')))
          .thenAnswer((invocation) async {
        currentAccessToken = invocation.namedArguments[#value] as String;
      });
      when(() => mockSecureStorage.write(key: 'refresh_token', value: any(named: 'value')))
          .thenAnswer((invocation) async {
        currentRefreshToken = invocation.namedArguments[#value] as String;
      });

      var refreshCallCount = 0;

      when(() => mockHttpClient.send(any())).thenAnswer((invocation) async {
        final request = invocation.positionalArguments.first as http.Request;
        if (request.url.path.contains(ApiEndpoints.refreshToken)) {
          refreshCallCount++;
          // Simulate slight network delay during refresh
          await Future.delayed(const Duration(milliseconds: 50));
          return http.StreamedResponse(
            Stream.value(utf8.encode(jsonEncode({
              'success': true,
              'data': {
                'accessToken': 'refreshed_token',
                'refreshToken': 'refreshed_refresh_token',
              },
            }))),
            200,
          );
        }

        // Check if token in header is refreshed
        if (request.headers['Authorization'] == 'Bearer refreshed_token') {
          return http.StreamedResponse(
            Stream.value(utf8.encode(jsonEncode({
              'success': true,
              'data': {'endpoint': request.url.path},
            }))),
            200,
          );
        }

        // Otherwise return 401
        return http.StreamedResponse(
          Stream.value(utf8.encode('{"message": "Unauthorized"}')),
          401,
        );
      });

      // Launch 3 requests simultaneously
      final future1 = apiClient.get('/endpoint1');
      final future2 = apiClient.get('/endpoint2');
      final future3 = apiClient.get('/endpoint3');

      final results = await Future.wait([future1, future2, future3]);

      expect(results[0]['endpoint'], contains('/endpoint1'));
      expect(results[1]['endpoint'], contains('/endpoint2'));
      expect(results[2]['endpoint'], contains('/endpoint3'));

      // Clean Code: Chỉ được gọi refresh đúng 1 lần duy nhất nhờ Completer/Lock
      expect(refreshCallCount, equals(1));
    });

    test('handles SocketException and TimeoutException with user-friendly ApiException', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);

      when(() => mockHttpClient.send(any())).thenThrow(
        const SocketException('Failed host lookup'),
      );

      expect(
        () => apiClient.get('/ping'),
        throwsA(
          isA<ApiException>()
              .having((e) => e.statusCode, 'statusCode', 0)
              .having((e) => e.message, 'message', contains('kết nối mạng')),
        ),
      );
    });

    test('accepts a bare JSON list and exposes it under data', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);
      when(() => mockHttpClient.send(any())).thenAnswer((_) async {
        return http.StreamedResponse(
          Stream.value(utf8.encode(jsonEncode([{'id': 1}]))),
          200,
        );
      });

      final response = await apiClient.get('/statistics/environmental');

      expect(response['data'], [{'id': 1}]);
    });

    group('when the connection is reset', () {
      const resetError = 'Connection reset by peer';

      setUp(() {
        when(() => mockSecureStorage.read(key: 'access_token'))
            .thenAnswer((_) async => null);
      });

      test('get() retries once and returns the second response', () async {
        var calls = 0;
        when(() => mockHttpClient.send(any())).thenAnswer((_) async {
          calls++;
          if (calls == 1) throw http.ClientException(resetError);
          return http.StreamedResponse(
            Stream.value(utf8.encode(jsonEncode({'success': true, 'data': {'id': '1'}}))),
            200,
          );
        });

        final response = await apiClient.get('/ping');

        expect(response['id'], '1');
        expect(calls, 2);
      });

      test('get() reports a friendly message when the retry also fails', () async {
        when(() => mockHttpClient.send(any())).thenThrow(http.ClientException(resetError));

        await expectLater(
          apiClient.get('/ping'),
          throwsA(
            isA<ApiException>()
                .having((e) => e.statusCode, 'statusCode', 0)
                .having((e) => e.message, 'message', 'Mất kết nối tới máy chủ. Vui lòng thử lại.'),
          ),
        );
        verify(() => mockHttpClient.send(any())).called(2);
      });

      test('post() does not retry, so a pump command is never sent twice', () async {
        when(() => mockHttpClient.send(any())).thenThrow(http.ClientException(resetError));

        await expectLater(
          apiClient.post('/control', body: {'action': 'ON'}),
          throwsA(isA<ApiException>()),
        );
        verify(() => mockHttpClient.send(any())).called(1);
      });

      test('get() does not retry after a timeout', () async {
        final client = ApiClient(
          client: mockHttpClient,
          storage: mockSecureStorage,
          timeout: const Duration(milliseconds: 1),
        );
        when(() => mockHttpClient.send(any())).thenAnswer(
          (_) => Completer<http.StreamedResponse>().future,
        );

        await expectLater(
          client.get('/ping'),
          throwsA(isA<ApiException>().having((e) => e.statusCode, 'statusCode', 408)),
        );
        verify(() => mockHttpClient.send(any())).called(1);
      });
    });

    test('saveUser and readUser serialize and deserialize JSON correctly', () async {
      final userMap = {'id': 'user_1', 'email': 'test@example.com'};
      when(() => mockSecureStorage.write(key: 'current_user', value: any(named: 'value')))
          .thenAnswer((_) async {});
      when(() => mockSecureStorage.read(key: 'current_user'))
          .thenAnswer((_) async => jsonEncode(userMap));

      await apiClient.saveUser(userMap);
      verify(() => mockSecureStorage.write(key: 'current_user', value: jsonEncode(userMap))).called(1);

      final readUser = await apiClient.readUser();
      expect(readUser, equals(userMap));
    });

    test('refresh failure due to network error retains tokens and does not emit sessionExpired', () async {
      when(() => mockSecureStorage.read(key: 'refresh_token'))
          .thenAnswer((_) async => 'existing_refresh_token');

      when(() => mockHttpClient.send(any())).thenThrow(
        const SocketException('No route to host'),
      );

      var sessionExpiredFired = false;
      final sub = apiClient.sessionExpired.listen((_) {
        sessionExpiredFired = true;
      });

      final refreshed = await apiClient.refreshSession();
      await Future<void>.delayed(const Duration(milliseconds: 10));

      expect(refreshed, isFalse);
      expect(sessionExpiredFired, isFalse);
      verifyNever(() => mockSecureStorage.delete(key: any(named: 'key')));

      await sub.cancel();
    });

    test('refresh failure with 401/403 clears tokens and emits sessionExpired', () async {
      when(() => mockSecureStorage.read(key: 'access_token'))
          .thenAnswer((_) async => null);
      when(() => mockSecureStorage.read(key: 'refresh_token'))
          .thenAnswer((_) async => 'expired_refresh_token');
      when(() => mockSecureStorage.delete(key: any(named: 'key')))
          .thenAnswer((_) async {});

      when(() => mockHttpClient.send(any())).thenAnswer((_) async {
        return http.StreamedResponse(
          Stream.value(utf8.encode(jsonEncode({'success': false, 'message': 'Refresh token expired'}))),
          401,
          headers: {'content-type': 'application/json'},
        );
      });

      var sessionExpiredFired = false;
      final sub = apiClient.sessionExpired.listen((_) {
        sessionExpiredFired = true;
      });

      final refreshed = await apiClient.refreshSession();
      await Future<void>.delayed(const Duration(milliseconds: 10));

      expect(refreshed, isFalse);
      expect(sessionExpiredFired, isTrue);
      verify(() => mockSecureStorage.delete(key: 'access_token')).called(1);
      verify(() => mockSecureStorage.delete(key: 'refresh_token')).called(1);
      verify(() => mockSecureStorage.delete(key: 'current_user')).called(1);

      await sub.cancel();
    });
  });
}
