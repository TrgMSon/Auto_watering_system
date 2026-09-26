import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/network/api_client.dart';
import '../models/login_request_model.dart';
import '../models/login_response_model.dart';
import '../models/user_model.dart';

abstract class AuthRemoteDataSource {
  Future<LoginResponseModel> login(LoginRequestModel request);
  Future<UserModel> getCurrentUser();
}

class AuthRemoteDataSourceImpl implements AuthRemoteDataSource {
  final ApiClient apiClient;

  const AuthRemoteDataSourceImpl({required this.apiClient});

  @override
  Future<LoginResponseModel> login(LoginRequestModel request) async {
    try {
      final response = await apiClient.dio.post(
        ApiEndpoints.login,
        data: request.toJson(),
      );
      return LoginResponseModel.fromJson(response.data);
    } catch (e) {
      throw ServerException(message: '\u0110\u0103ng nh\u1eadp th\u1ea5t b\u1ea1i: \$e');
    }
  }

  @override
  Future<UserModel> getCurrentUser() async {
    try {
      final response = await apiClient.dio.get(ApiEndpoints.currentUser);
      return UserModel.fromJson(response.data);
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 l\u1ea5y th\u00f4ng tin user: \$e');
    }
  }
}
