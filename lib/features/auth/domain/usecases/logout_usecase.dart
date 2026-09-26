import '../repositories/auth_repository.dart';

class LogoutUseCase {
  final AuthRepository repository;
  const LogoutUseCase(this.repository);

  Future<void> call() => repository.logout();
}
