import 'dart:convert';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:http/http.dart' as http;

import '../constants/api_endpoints.dart';

class ApiException implements Exception {
  final int statusCode;
  final String message;

  const ApiException(this.statusCode, this.message);

  @override
  String toString() => message;
}

class ApiClient {
  ApiClient({
    http.Client? client,
    FlutterSecureStorage? storage,
  })  : _client = client ?? http.Client(),
        _storage = storage ?? const FlutterSecureStorage();

  final http.Client _client;
  final FlutterSecureStorage _storage;

  Future<Map<String, dynamic>> get(
    String path, {
    Map<String, String>? queryParameters,
  }) =>
      _send('GET', path, queryParameters: queryParameters);

  Future<Map<String, dynamic>> post(
    String path, {
    Object? body,
  }) =>
      _send('POST', path, body: body);

  Future<Map<String, dynamic>> put(
    String path, {
    Object? body,
  }) =>
      _send('PUT', path, body: body);

  Future<void> delete(String path) async {
    await _send('DELETE', path);
  }

  Future<Map<String, dynamic>> _send(
    String method,
    String path, {
    Object? body,
    Map<String, String>? queryParameters,
    bool retryUnauthorized = true,
  }) async {
    final uri = Uri.parse('${ApiEndpoints.baseUrl}$path')
        .replace(queryParameters: queryParameters);
    final token = await _storage.read(key: _accessTokenKey);
    final headers = <String, String>{'Accept': 'application/json'};
    if (body != null) headers['Content-Type'] = 'application/json';
    if (token != null && token.isNotEmpty) {
      headers['Authorization'] = 'Bearer $token';
    }

    final request = http.Request(method, uri)..headers.addAll(headers);
    if (body != null) request.body = jsonEncode(body);
    final streamedResponse = await _client.send(request).timeout(
      const Duration(seconds: 15),
    );
    final response = await http.Response.fromStream(streamedResponse);

    if (response.statusCode == 401 &&
        retryUnauthorized &&
        path != ApiEndpoints.login &&
        path != ApiEndpoints.refreshToken) {
      final refreshed = await _refreshToken();
      if (refreshed) {
        return _send(
          method,
          path,
          body: body,
          queryParameters: queryParameters,
          retryUnauthorized: false,
        );
      }
    }

    final decoded = response.body.isEmpty
        ? <String, dynamic>{}
        : jsonDecode(response.body);
    if (decoded is! Map<String, dynamic>) {
      throw ApiException(response.statusCode, 'Phản hồi API không hợp lệ.');
    }
    if (response.statusCode < 200 ||
        response.statusCode >= 300 ||
        decoded['success'] == false) {
      throw ApiException(
        response.statusCode,
        decoded['message']?.toString() ?? 'Yêu cầu API thất bại.',
      );
    }

    final data = decoded['data'];
    if (data == null) return <String, dynamic>{};
    if (data is Map<String, dynamic>) return data;
    return <String, dynamic>{'data': data};
  }

  Future<bool> _refreshToken() async {
    final refreshToken = await _storage.read(key: _refreshTokenKey);
    if (refreshToken == null || refreshToken.isEmpty) {
      await clearTokens();
      return false;
    }

    try {
      final response = await _send(
        'POST',
        ApiEndpoints.refreshToken,
        body: {'refreshToken': refreshToken},
        retryUnauthorized: false,
      );
      final accessToken = response['accessToken']?.toString();
      final rotatedRefreshToken = response['refreshToken']?.toString();
      if (accessToken == null || rotatedRefreshToken == null) {
        await clearTokens();
        return false;
      }
      await saveTokens(accessToken, rotatedRefreshToken);
      return true;
    } on ApiException {
      await clearTokens();
      return false;
    }
  }

  Future<void> saveTokens(String accessToken, String refreshToken) async {
    await _storage.write(key: _accessTokenKey, value: accessToken);
    await _storage.write(key: _refreshTokenKey, value: refreshToken);
  }

  Future<String?> get accessToken => _storage.read(key: _accessTokenKey);

  Future<String?> get refreshToken => _storage.read(key: _refreshTokenKey);

  Future<bool> refreshSession() => _refreshToken();

  Future<void> clearTokens() async {
    await _storage.delete(key: _accessTokenKey);
    await _storage.delete(key: _refreshTokenKey);
    await _storage.delete(key: _userKey);
  }

  Future<void> saveUser(Map<String, dynamic> user) async {
    await _storage.write(key: _userKey, value: jsonEncode(user));
  }

  Future<Map<String, dynamic>?> readUser() async {
    final value = await _storage.read(key: _userKey);
    if (value == null) return null;
    final decoded = jsonDecode(value);
    return decoded is Map<String, dynamic> ? decoded : null;
  }

  static const _accessTokenKey = 'access_token';
  static const _refreshTokenKey = 'refresh_token';
  static const _userKey = 'current_user';
}
