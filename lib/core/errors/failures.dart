import 'package:equatable/equatable.dart';

sealed class Failure extends Equatable {
  final String message;
  const Failure({required this.message});

  @override
  List<Object> get props => [message];
}

class ServerFailure extends Failure {
  const ServerFailure({required super.message});
}

class CacheFailure extends Failure {
  const CacheFailure({required super.message});
}

class NetworkFailure extends Failure {
  const NetworkFailure({super.message = 'Không có kết nối mạng'});
}

class UnauthorizedFailure extends Failure {
  const UnauthorizedFailure({super.message = 'Phiên đăng nhập đã hết hạn'});
}
