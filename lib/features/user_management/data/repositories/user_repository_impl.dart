import '../../../auth/domain/entities/user.dart';
import '../../domain/entities/managed_user.dart';
import '../../domain/repositories/user_repository.dart';
import '../datasources/user_remote_datasource.dart';

class UserRepositoryImpl implements UserRepository {
  final UserRemoteDataSource remoteDataSource;
  const UserRepositoryImpl({required this.remoteDataSource});

  @override
  Future<List<ManagedUser>> getUsers() => remoteDataSource.getUsers();

  @override
  Future<void> updateUserRole({required int userId, required UserRole newRole}) =>
      remoteDataSource.updateUserRole(userId: userId, newRole: newRole);

  @override
  Future<void> deleteUser(int userId) => remoteDataSource.deleteUser(userId);

  @override
  Future<void> toggleUserActive({required int userId, required bool isActive}) =>
      remoteDataSource.toggleUserActive(userId: userId, isActive: isActive);
}
