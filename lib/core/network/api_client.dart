import 'package:dio/dio.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import '../constants/api_endpoints.dart';
import '../constants/app_constants.dart';
import '../errors/exceptions.dart';

class ApiClient {
  late final Dio dio;
  final FlutterSecureStorage secureStorage;

  ApiClient({required this.secureStorage}) {
    dio = Dio(BaseOptions(
      baseUrl: ApiEndpoints.baseUrl,
      connectTimeout: const Duration(seconds: 15),
      receiveTimeout: const Duration(seconds: 15),
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
    ));

    dio.interceptors.add(_authInterceptor());
    dio.interceptors.add(LogInterceptor(
      requestBody: true,
      responseBody: true,
      logPrint: (obj) => print('[DIO] $obj'),
    ));
  }

  QueuedInterceptorsWrapper _authInterceptor() {
    return QueuedInterceptorsWrapper(
      onRequest: (options, handler) async {
        if (options.path.contains('/auth/login') ||
            options.path.contains('/auth/register') ||
            options.path.contains('/auth/refresh')) {
          return handler.next(options);
        }

        final accessToken = await secureStorage.read(
          key: AppConstants.jwtAccessTokenKey,
        );
        if (accessToken != null) {
          options.headers['Authorization'] = 'Bearer $accessToken';
        }
        return handler.next(options);
      },
      onError: (DioException error, handler) async {
        if (error.response?.statusCode == 401 &&
            !error.requestOptions.path.contains('/auth/')) {
          final refreshed = await _refreshToken();
          if (refreshed) {
            final newAccessToken = await secureStorage.read(
              key: AppConstants.jwtAccessTokenKey,
            );
            error.requestOptions.headers['Authorization'] =
                'Bearer $newAccessToken';
            try {
              final response = await dio.fetch(error.requestOptions);
              return handler.resolve(response);
            } on DioException catch (e) {
              return handler.next(e);
            }
          } else {
            return handler.next(
              DioException(
                requestOptions: error.requestOptions,
                error: const UnauthorizedException(),
                type: DioExceptionType.unknown,
              ),
            );
          }
        }
        return handler.next(error);
      },
    );
  }

  Future<bool> _refreshToken() async {
    final refreshToken = await secureStorage.read(
      key: AppConstants.jwtRefreshTokenKey,
    );
    if (refreshToken == null) return false;

    try {
      final refreshDio = Dio(BaseOptions(baseUrl: ApiEndpoints.baseUrl));
      final response = await refreshDio.post(
        ApiEndpoints.refreshToken,
        data: {'refreshToken': refreshToken},
      );

      if (response.statusCode == 200) {
        await secureStorage.write(
          key: AppConstants.jwtAccessTokenKey,
          value: response.data['accessToken'],
        );
        await secureStorage.write(
          key: AppConstants.jwtRefreshTokenKey,
          value: response.data['refreshToken'],
        );
        return true;
      }
      return false;
    } catch (_) {
      await secureStorage.deleteAll();
      return false;
    }
  }
}
