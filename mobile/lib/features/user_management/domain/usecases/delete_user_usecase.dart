import '../repositories/user_repository.dart';

class DeleteUserUseCase {
  final UserRepository repository;
  const DeleteUserUseCase(this.repository);
  Future<void> call(int userId) => repository.deleteUser(userId);
}
