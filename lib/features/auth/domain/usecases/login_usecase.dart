import '../entities/user.dart';
import '../repositories/auth_repository.dart';

class LoginUseCase {
  final AuthRepository repository;
  const LoginUseCase(this.repository);

  Future<User> call({required String username, required String password}) {
    return repository.login(username: username, password: password);
  }
}
