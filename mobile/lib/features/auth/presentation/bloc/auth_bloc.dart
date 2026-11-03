import 'dart:async';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../domain/entities/user.dart';
import '../../domain/usecases/get_current_user_usecase.dart';
import '../../domain/usecases/login_usecase.dart';
import '../../domain/usecases/logout_usecase.dart';
import 'auth_event.dart';
import 'auth_state.dart';

class AuthBloc extends Bloc<AuthEvent, AuthState> {
  final LoginUseCase loginUseCase;
  final LogoutUseCase logoutUseCase;
  final GetCurrentUserUseCase getCurrentUserUseCase;

  StreamSubscription<void>? _sessionSubscription;

  AuthBloc({
    required this.loginUseCase,
    required this.logoutUseCase,
    required this.getCurrentUserUseCase,
    Stream<void>? sessionExpiredStream,
  }) : super(const AuthInitial()) {
    on<AuthCheckRequested>(_onCheckRequested);
    on<AuthLoginRequested>(_onLoginRequested);
    on<AuthLogoutRequested>(_onLogoutRequested);
    on<AuthSessionExpired>(_onSessionExpired);

    _sessionSubscription = sessionExpiredStream?.listen((_) {
      add(const AuthSessionExpired());
    });
  }

  Future<void> _onCheckRequested(
    AuthCheckRequested event,
    Emitter<AuthState> emit,
  ) async {
    emit(const AuthChecking());
    try {
      final user = await getCurrentUserUseCase();
      if (user == null) {
        emit(const AuthUnauthenticated());
        return;
      }
      await _handleUserRole(user, emit);
    } catch (_) {
      emit(const AuthUnauthenticated());
    }
  }

  Future<void> _onLoginRequested(
    AuthLoginRequested event,
    Emitter<AuthState> emit,
  ) async {
    emit(const AuthLoading());
    try {
      final user = await loginUseCase(
        username: event.username,
        password: event.password,
      );
      await _handleUserRole(user, emit);
    } catch (e) {
      emit(const AuthError(
        message: 'Đăng nhập thất bại. Vui lòng kiểm tra lại thông tin.',
      ));
    }
  }

  Future<void> _handleUserRole(User user, Emitter<AuthState> emit) async {
    if (user.isAdmin) {
      try {
        await logoutUseCase();
      } catch (_) {
        // Đảm bảo không bị crash khi xoá phiên
      }
      emit(const AuthError(
        message: 'Tài khoản quản trị vui lòng sử dụng trang web quản trị.',
      ));
      return;
    }
    emit(AuthAuthenticated(user: user));
  }

  Future<void> _onLogoutRequested(
    AuthLogoutRequested event,
    Emitter<AuthState> emit,
  ) async {
    try {
      await logoutUseCase();
    } catch (_) {
      // Đảm bảo không bị crash khi API đăng xuất gặp lỗi
    } finally {
      emit(const AuthUnauthenticated());
    }
  }

  void _onSessionExpired(
    AuthSessionExpired event,
    Emitter<AuthState> emit,
  ) {
    emit(const AuthUnauthenticated());
  }

  @override
  Future<void> close() {
    _sessionSubscription?.cancel();
    return super.close();
  }
}
