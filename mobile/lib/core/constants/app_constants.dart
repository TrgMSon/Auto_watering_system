class AppConstants {
  AppConstants._();

  static const String appName = 'Smart Watering';
  static const String jwtAccessTokenKey = 'jwt_access_token';
  static const String jwtRefreshTokenKey = 'jwt_refresh_token';
  static const String userRoleKey = 'user_role';

  static const double moistureWarningThreshold = 30.0;
  static const double moistureCriticalThreshold = 15.0;
  static const double temperatureWarningThreshold = 40.0;
  static const double humidityWarningThreshold = 20.0;
}
