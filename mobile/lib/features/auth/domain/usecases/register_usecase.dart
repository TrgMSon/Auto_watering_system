import '../repositories/auth_repository.dart';

class RegisterUseCase {
  final AuthRepository repository;
  const RegisterUseCase(this.repository);

  Future<void> call({required String email, required String password}) {
    return repository.register(email: email, password: password);
  }
}
