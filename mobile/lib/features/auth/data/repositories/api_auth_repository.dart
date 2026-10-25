import 'dart:convert';

import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/entities/user.dart';
import '../../domain/repositories/auth_repository.dart';

class ApiAuthRepository implements AuthRepository {
  ApiAuthRepository(this._apiClient);

  final ApiClient _apiClient;

  @override
  Future<User> login({required String username, required String password}) async {
    final response = await _apiClient.post(
      ApiEndpoints.login,
      body: {'email': username, 'password': password},
    );
    final accessToken = response['accessToken']?.toString();
    final refreshToken = response['refreshToken']?.toString();
    if (accessToken == null || refreshToken == null) {
      throw const ApiException(500, 'Phản hồi đăng nhập thiếu token.');
    }
    await _apiClient.saveTokens(accessToken, refreshToken);
    final userJson = response['userResponse'];
    final user = _userFromResponse(
      userJson is Map<String, dynamic> ? userJson : const {},
      accessToken,
      response['role']?.toString(),
    );
    await _apiClient.saveUser(_userToJson(user));
    return user;
  }

  @override
  Future<void> logout() async {
    final refreshToken = await _apiClient.refreshToken;
    try {
      if (refreshToken != null && refreshToken.isNotEmpty) {
        await _apiClient.post(
          ApiEndpoints.logout,
          body: {'refreshToken': refreshToken},
        );
      }
    } finally {
      await _apiClient.clearTokens();
    }
  }

  @override
  Future<User?> getCurrentUser() async {
    var token = await _apiClient.accessToken;
    if (token == null) return null;
    if (!_isTokenUsable(token)) {
      if (!await _apiClient.refreshSession()) return null;
      token = await _apiClient.accessToken;
    }
    if (token == null) return null;
    final storedUser = await _apiClient.readUser();
    if (storedUser == null) return null;
    return _userFromResponse(storedUser, token, storedUser['role']?.toString());
  }

  @override
  Future<bool> isLoggedIn() async => await getCurrentUser() != null;

  bool _isTokenUsable(String token) {
    try {
      final payload = token.split('.')[1];
      final claims = jsonDecode(
        utf8.decode(base64Url.decode(base64Url.normalize(payload))),
      ) as Map<String, dynamic>;
      final expiration = claims['exp'] as int?;
      return expiration != null &&
          DateTime.now().millisecondsSinceEpoch ~/ 1000 < expiration;
    } on FormatException {
      return false;
    } on TypeError {
      return false;
    } on RangeError {
      return false;
    }
  }

  User _userFromResponse(
    Map<String, dynamic> json,
    String token,
    String? role,
  ) {
    final email = json['email']?.toString() ?? '';
    final id = json['id'] ?? _userIdFromToken(token);
    return User(
      id: id is int ? id : int.tryParse(id.toString()) ?? 0,
      username: email,
      email: email,
      fullName: null,
      role: (role ?? json['role']?.toString() ?? 'USER').toUpperCase() == 'ADMIN'
          ? UserRole.admin
          : UserRole.user,
    );
  }

  int _userIdFromToken(String token) {
    try {
      final payload = token.split('.')[1];
      final claims = jsonDecode(
        utf8.decode(base64Url.decode(base64Url.normalize(payload))),
      ) as Map<String, dynamic>;
      final id = claims['userId'];
      return id is int ? id : int.tryParse(id.toString()) ?? 0;
    } on FormatException {
      return 0;
    } on TypeError {
      return 0;
    } on RangeError {
      return 0;
    }
  }

  Map<String, dynamic> _userToJson(User user) => {
        'id': user.id,
        'email': user.email,
        'phoneNumber': user.fullName,
        'role': user.role == UserRole.admin ? 'ADMIN' : 'USER',
      };
}
