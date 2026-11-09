import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/auth/domain/usecases/register_usecase.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_state.dart';

class MockRegisterUseCase extends Mock implements RegisterUseCase {}

void main() {
  late MockRegisterUseCase mockRegisterUseCase;

  const submitted = RegisterSubmitted(
    email: 'new@test.com',
    password: 'password123',
  );

  setUp(() {
    mockRegisterUseCase = MockRegisterUseCase();
  });

  void stubRegister(Future<void> Function() answer) {
    when(() => mockRegisterUseCase(
          email: 'new@test.com',
          password: 'password123',
        )).thenAnswer((_) => answer());
  }

  blocTest<RegisterBloc, RegisterState>(
    'emits [RegisterLoading, RegisterSuccess] when registration succeeds',
    build: () {
      stubRegister(() async {});
      return RegisterBloc(registerUseCase: mockRegisterUseCase);
    },
    act: (bloc) => bloc.add(submitted),
    expect: () => [
      const RegisterLoading(),
      const RegisterSuccess(email: 'new@test.com'),
    ],
  );

  blocTest<RegisterBloc, RegisterState>(
    'shows the reason from the backend when the API rejects the request',
    build: () {
      stubRegister(() async {
        throw const ApiException(409, 'Email này đã được đăng ký trong hệ thống.');
      });
      return RegisterBloc(registerUseCase: mockRegisterUseCase);
    },
    act: (bloc) => bloc.add(submitted),
    expect: () => [
      const RegisterLoading(),
      const RegisterFailure(message: 'Email này đã được đăng ký trong hệ thống.'),
    ],
  );

  blocTest<RegisterBloc, RegisterState>(
    'shows a generic message for unexpected errors',
    build: () {
      stubRegister(() async {
        throw Exception('unexpected');
      });
      return RegisterBloc(registerUseCase: mockRegisterUseCase);
    },
    act: (bloc) => bloc.add(submitted),
    expect: () => [
      const RegisterLoading(),
      const RegisterFailure(message: 'Đăng ký thất bại. Vui lòng thử lại.'),
    ],
  );
}
