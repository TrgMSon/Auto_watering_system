import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/errors/exceptions.dart';
import '../../../../core/network/api_client.dart';
import '../../../auth/domain/entities/user.dart';
import '../models/user_admin_model.dart';

abstract class UserRemoteDataSource {
  Future<List<UserAdminModel>> getUsers();
  Future<void> updateUserRole({required int userId, required UserRole newRole});
  Future<void> deleteUser(int userId);
  Future<void> toggleUserActive({required int userId, required bool isActive});
}

class UserRemoteDataSourceImpl implements UserRemoteDataSource {
  final ApiClient apiClient;
  const UserRemoteDataSourceImpl({required this.apiClient});

  @override
  Future<List<UserAdminModel>> getUsers() async {
    try {
      final response = await apiClient.dio.get(ApiEndpoints.users);
      final List<dynamic> data = response.data as List<dynamic>;
      return data.map((j) => UserAdminModel.fromJson(j as Map<String, dynamic>)).toList();
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 t\u1ea3i danh s\u00e1ch ng\u01b0\u1eddi d\u00f9ng: \$e');
    }
  }

  @override
  Future<void> updateUserRole({required int userId, required UserRole newRole}) async {
    try {
      await apiClient.dio.put(
        '\${ApiEndpoints.users}/\$userId/role',
        data: {'role': newRole == UserRole.admin ? 'ADMIN' : 'USER'},
      );
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 c\u1eadp nh\u1eadt quy\u1ec1n: \$e');
    }
  }

  @override
  Future<void> deleteUser(int userId) async {
    try {
      await apiClient.dio.delete('\${ApiEndpoints.users}/\$userId');
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 x\u00f3a ng\u01b0\u1eddi d\u00f9ng: \$e');
    }
  }

  @override
  Future<void> toggleUserActive({required int userId, required bool isActive}) async {
    try {
      await apiClient.dio.put(
        '\${ApiEndpoints.users}/\$userId/status',
        data: {'active': isActive},
      );
    } catch (e) {
      throw ServerException(message: 'Kh\u00f4ng th\u1ec3 c\u1eadp nh\u1eadt tr\u1ea1ng th\u00e1i: \$e');
    }
  }
}
