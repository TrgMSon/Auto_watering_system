import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:http/http.dart' as http;

import '../constants/api_endpoints.dart';

/// Đại diện cho lỗi phản hồi từ API hoặc lỗi kết nối mạng
class ApiException implements Exception {
  final int statusCode;
  final String message;

  const ApiException(this.statusCode, this.message);

  @override
  String toString() => message;
}

/// Client quản lý kết nối HTTP, tự động đính kèm Token và làm mới phiên đăng nhập
class ApiClient {
  ApiClient({
    http.Client? client,
    FlutterSecureStorage? storage,
    Duration timeout = _defaultTimeout,
  })  : _client = client ?? http.Client(),
        _storage = storage ?? const FlutterSecureStorage(),
        _timeout = timeout;

  // --- Constants ---
  static const Duration _defaultTimeout = Duration(seconds: 15);
  static const String _accessTokenKey = 'access_token';
  static const String _refreshTokenKey = 'refresh_token';
  static const String _userKey = 'current_user';

  final http.Client _client;
  final FlutterSecureStorage _storage;
  final Duration _timeout;

  /// Completer đóng vai trò khóa đồng bộ tránh Race Condition khi nhiều request 401 đồng thời
  Completer<bool>? _refreshCompleter;

  // --- Public HTTP Methods ---

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

  // --- Core Request Dispatcher ---

  Future<Map<String, dynamic>> _send(
    String method,
    String path, {
    Object? body,
    Map<String, String>? queryParameters,
    bool retryUnauthorized = true,
  }) async {
    final uri = _buildUri(path, queryParameters);
    final headers = await _buildHeaders(hasBody: body != null);

    final response = await _executeRawRequest(
      method: method,
      uri: uri,
      headers: headers,
      body: body,
    );

    if (_shouldRetryRefresh(response.statusCode, retryUnauthorized, path)) {
      final refreshed = await _refreshTokenSafe();
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

    return _processResponse(response);
  }

  // --- Private Helpers (Do One Thing & Under 20 Lines) ---

  Uri _buildUri(String path, Map<String, String>? queryParameters) {
    return Uri.parse('${ApiEndpoints.baseUrl}$path')
        .replace(queryParameters: queryParameters);
  }

  Future<Map<String, String>> _buildHeaders({required bool hasBody}) async {
    final headers = <String, String>{'Accept': 'application/json'};
    if (hasBody) {
      headers['Content-Type'] = 'application/json';
    }

    final token = await _storage.read(key: _accessTokenKey);
    if (token != null && token.isNotEmpty) {
      headers['Authorization'] = 'Bearer $token';
    }
    return headers;
  }

  Future<http.Response> _executeRawRequest({
    required String method,
    required Uri uri,
    required Map<String, String> headers,
    Object? body,
  }) async {
    try {
      final request = http.Request(method, uri)..headers.addAll(headers);
      if (body != null) {
        request.body = jsonEncode(body);
      }

      final streamedResponse = await _client.send(request).timeout(_timeout);
      return await http.Response.fromStream(streamedResponse);
    } on SocketException {
      throw const ApiException(0, 'Không có kết nối mạng. Vui lòng kiểm tra lại.');
    } on TimeoutException {
      throw const ApiException(408, 'Hết thời gian chờ phản hồi từ máy chủ.');
    } on http.ClientException catch (e) {
      throw ApiException(0, 'Lỗi kết nối: ${e.message}');
    }
  }

  bool _shouldRetryRefresh(int statusCode, bool retryUnauthorized, String path) {
    return statusCode == 401 &&
        retryUnauthorized &&
        path != ApiEndpoints.login &&
        path != ApiEndpoints.refreshToken;
  }

  /// Khóa đồng bộ Refresh Token: gom các request 401 đồng thời vào một lời gọi duy nhất
  Future<bool> _refreshTokenSafe() async {
    if (_refreshCompleter != null) {
      return _refreshCompleter!.future;
    }

    final completer = Completer<bool>();
    _refreshCompleter = completer;

    try {
      final success = await _executeRefreshToken();
      completer.complete(success);
      return success;
    } catch (_) {
      completer.complete(false);
      return false;
    } finally {
      _refreshCompleter = null;
    }
  }

  Future<bool> _executeRefreshToken() async {
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

  Map<String, dynamic> _processResponse(http.Response response) {
    if (response.bodyBytes.isEmpty) {
      return <String, dynamic>{};
    }

    dynamic decoded;
    try {
      // Decode bằng utf8 để chống lỗi hiển thị dấu tiếng Việt
      final bodyString = utf8.decode(response.bodyBytes);
      decoded = jsonDecode(bodyString);
    } on FormatException {
      throw ApiException(response.statusCode, 'Phản hồi từ máy chủ không đúng định dạng JSON.');
    }

    if (decoded is! Map<String, dynamic>) {
      throw ApiException(response.statusCode, 'Phản hồi API không hợp lệ.');
    }

    final isSuccessStatus = response.statusCode >= 200 && response.statusCode < 300;
    final isSuccessPayload = decoded['success'] != false;

    if (!isSuccessStatus || !isSuccessPayload) {
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

  // --- Session & Storage Management ---

  Future<void> saveTokens(String accessToken, String refreshToken) async {
    await _storage.write(key: _accessTokenKey, value: accessToken);
    await _storage.write(key: _refreshTokenKey, value: refreshToken);
  }

  Future<String?> get accessToken => _storage.read(key: _accessTokenKey);

  Future<String?> get refreshToken => _storage.read(key: _refreshTokenKey);

  Future<bool> refreshSession() => _refreshTokenSafe();

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
    try {
      final decoded = jsonDecode(value);
      return decoded is Map<String, dynamic> ? decoded : null;
    } on FormatException {
      return null;
    }
  }
}
