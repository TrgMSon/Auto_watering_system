import 'dart:async';
import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/auth/domain/entities/user.dart';
import 'package:watering_app/features/auth/domain/usecases/get_current_user_usecase.dart';
import 'package:watering_app/features/auth/domain/usecases/login_usecase.dart';
import 'package:watering_app/features/auth/domain/usecases/logout_usecase.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_state.dart';

class MockLoginUseCase extends Mock implements LoginUseCase {}

class MockLogoutUseCase extends Mock implements LogoutUseCase {}

class MockGetCurrentUserUseCase extends Mock implements GetCurrentUserUseCase {}

void main() {
  late MockLoginUseCase mockLoginUseCase;
  late MockLogoutUseCase mockLogoutUseCase;
  late MockGetCurrentUserUseCase mockGetCurrentUserUseCase;

  const testUser = User(
    id: 1,
    username: 'testuser',
    email: 'test@example.com',
    fullName: 'Test User',
    role: UserRole.user,
  );

  setUp(() {
    mockLoginUseCase = MockLoginUseCase();
    mockLogoutUseCase = MockLogoutUseCase();
    mockGetCurrentUserUseCase = MockGetCurrentUserUseCase();
  });

  group('AuthBloc - AuthCheckRequested', () {
    blocTest<AuthBloc, AuthState>(
      'emits [AuthChecking, AuthAuthenticated] when user is present',
      build: () {
        when(() => mockGetCurrentUserUseCase())
            .thenAnswer((_) async => testUser);
        return AuthBloc(
          loginUseCase: mockLoginUseCase,
          logoutUseCase: mockLogoutUseCase,
          getCurrentUserUseCase: mockGetCurrentUserUseCase,
        );
      },
      act: (bloc) => bloc.add(const AuthCheckRequested()),
      expect: () => [
        const AuthChecking(),
        const AuthAuthenticated(user: testUser),
      ],
    );

    blocTest<AuthBloc, AuthState>(
      'emits [AuthChecking, AuthUnauthenticated] when user is null',
      build: () {
        when(() => mockGetCurrentUserUseCase()).thenAnswer((_) async => null);
        return AuthBloc(
          loginUseCase: mockLoginUseCase,
          logoutUseCase: mockLogoutUseCase,
          getCurrentUserUseCase: mockGetCurrentUserUseCase,
        );
      },
      act: (bloc) => bloc.add(const AuthCheckRequested()),
      expect: () => [
        const AuthChecking(),
        const AuthUnauthenticated(),
      ],
    );
  });

  group('AuthBloc - AuthLogoutRequested', () {
    blocTest<AuthBloc, AuthState>(
      'always emits AuthUnauthenticated even when logoutUseCase throws an exception',
      build: () {
        when(() => mockLogoutUseCase()).thenThrow(Exception('Logout failed'));
        return AuthBloc(
          loginUseCase: mockLoginUseCase,
          logoutUseCase: mockLogoutUseCase,
          getCurrentUserUseCase: mockGetCurrentUserUseCase,
        );
      },
      act: (bloc) => bloc.add(const AuthLogoutRequested()),
      expect: () => [
        const AuthUnauthenticated(),
      ],
    );
  });

  group('AuthBloc - Session Expiration', () {
    blocTest<AuthBloc, AuthState>(
      'emits AuthUnauthenticated on AuthSessionExpired',
      build: () => AuthBloc(
        loginUseCase: mockLoginUseCase,
        logoutUseCase: mockLogoutUseCase,
        getCurrentUserUseCase: mockGetCurrentUserUseCase,
      ),
      act: (bloc) => bloc.add(const AuthSessionExpired()),
      expect: () => [
        const AuthUnauthenticated(),
      ],
    );

    test('listens to sessionExpiredStream and emits AuthUnauthenticated when session expires', () async {
      final sessionController = StreamController<void>();

      final bloc = AuthBloc(
        loginUseCase: mockLoginUseCase,
        logoutUseCase: mockLogoutUseCase,
        getCurrentUserUseCase: mockGetCurrentUserUseCase,
        sessionExpiredStream: sessionController.stream,
      );

      final states = <AuthState>[];
      final sub = bloc.stream.listen(states.add);

      sessionController.add(null);
      await pumpEventQueue();

      expect(states, equals([const AuthUnauthenticated()]));

      await sub.cancel();
      await bloc.close();
      await sessionController.close();
    });
  });
}

