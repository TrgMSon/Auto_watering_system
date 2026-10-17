import '../../../auth/domain/entities/user.dart';
import '../entities/managed_user.dart';

abstract class UserRepository {
  Future<List<ManagedUser>> getUsers();
  Future<void> updateUserRole({required int userId, required UserRole newRole});
  Future<void> deleteUser(int userId);
  Future<void> toggleUserActive({required int userId, required bool isActive});
}
