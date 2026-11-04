import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/usecases/register_usecase.dart';
import 'register_event.dart';
import 'register_state.dart';

class RegisterBloc extends Bloc<RegisterEvent, RegisterState> {
  final RegisterUseCase registerUseCase;

  RegisterBloc({required this.registerUseCase}) : super(const RegisterInitial()) {
    on<RegisterSubmitted>(_onSubmitted);
  }

  Future<void> _onSubmitted(
    RegisterSubmitted event,
    Emitter<RegisterState> emit,
  ) async {
    emit(const RegisterLoading());
    try {
      await registerUseCase(email: event.email, password: event.password);
      emit(RegisterSuccess(email: event.email));
    } on ApiException catch (e) {
      // Backend trả lý do cụ thể, ví dụ email đã được đăng ký.
      emit(RegisterFailure(message: e.message));
    } catch (_) {
      emit(const RegisterFailure(message: 'Đăng ký thất bại. Vui lòng thử lại.'));
    }
  }
}
