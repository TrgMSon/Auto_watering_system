import '../entities/managed_user.dart';
import '../repositories/user_repository.dart';

class GetUsersUseCase {
  final UserRepository repository;
  const GetUsersUseCase(this.repository);
  Future<List<ManagedUser>> call() => repository.getUsers();
}
