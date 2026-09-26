class ServerException implements Exception {
  final String message;
  final int? statusCode;

  const ServerException({required this.message, this.statusCode});

  @override
  String toString() => 'ServerException(message: $message, statusCode: $statusCode)';
}

class CacheException implements Exception {
  final String message;
  const CacheException({required this.message});
}

class NetworkException implements Exception {
  final String message;
  const NetworkException({required this.message});
}

class UnauthorizedException implements Exception {
  final String message;
  const UnauthorizedException({this.message = 'Phiên đăng nhập đã hết hạn'});
}
