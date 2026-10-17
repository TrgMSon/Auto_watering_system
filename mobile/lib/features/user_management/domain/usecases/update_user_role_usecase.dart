import '../../../auth/domain/entities/user.dart';
import '../repositories/user_repository.dart';

class UpdateUserRoleUseCase {
  final UserRepository repository;
  const UpdateUserRoleUseCase(this.repository);

  Future<void> call({required int userId, required UserRole newRole}) =>
      repository.updateUserRole(userId: userId, newRole: newRole);
}
