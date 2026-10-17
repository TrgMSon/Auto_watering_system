class ApiEndpoints {
  ApiEndpoints._();

  static const String baseUrl = 'http://10.0.2.2:8080/api/v1';
  static const String wsUrl = 'ws://10.0.2.2:8080/ws-telemetry';

  static const String login = '/auth/login';
  static const String register = '/auth/register';
  static const String refreshToken = '/auth/refresh';
  static const String currentUser = '/auth/me';

  static const String dashboardSummary = '/dashboard/summary';
  static const String toggleAutoWatering = '/dashboard/auto-watering';

  static const String sensorReadings = '/sensors/readings';
  static const String sensorHistory = '/sensors/history';

  static const String devices = '/devices';

  static const String users = '/admin/users';
}
