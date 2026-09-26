import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import '../../../../core/constants/app_constants.dart';
import '../../domain/entities/user.dart';
import '../../domain/repositories/auth_repository.dart';
import '../datasources/auth_remote_datasource.dart';
import '../models/login_request_model.dart';

class AuthRepositoryImpl implements AuthRepository {
  final AuthRemoteDataSource remoteDataSource;
  final FlutterSecureStorage secureStorage;

  const AuthRepositoryImpl({
    required this.remoteDataSource,
    required this.secureStorage,
  });

  @override
  Future<User> login({
    required String username,
    required String password,
  }) async {
    final loginResponse = await remoteDataSource.login(
      LoginRequestModel(username: username, password: password),
    );

    await secureStorage.write(
      key: AppConstants.jwtAccessTokenKey,
      value: loginResponse.accessToken,
    );
    await secureStorage.write(
      key: AppConstants.jwtRefreshTokenKey,
      value: loginResponse.refreshToken,
    );

    return remoteDataSource.getCurrentUser();
  }

  @override
  Future<void> logout() async {
    await secureStorage.delete(key: AppConstants.jwtAccessTokenKey);
    await secureStorage.delete(key: AppConstants.jwtRefreshTokenKey);
  }

  @override
  Future<User?> getCurrentUser() async {
    final hasToken = await isLoggedIn();
    if (!hasToken) return null;

    try {
      return await remoteDataSource.getCurrentUser();
    } catch (_) {
      return null;
    }
  }

  @override
  Future<bool> isLoggedIn() async {
    final token = await secureStorage.read(
      key: AppConstants.jwtAccessTokenKey,
    );
    return token != null;
  }
}
