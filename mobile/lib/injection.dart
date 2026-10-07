import 'package:get_it/get_it.dart';

import 'core/mock/mock_repositories.dart';

import 'features/auth/domain/repositories/auth_repository.dart';
import 'features/auth/domain/usecases/login_usecase.dart';
import 'features/auth/domain/usecases/logout_usecase.dart';
import 'features/auth/domain/usecases/get_current_user_usecase.dart';
import 'features/auth/presentation/bloc/auth_bloc.dart';

import 'features/dashboard/domain/repositories/dashboard_repository.dart';
import 'features/dashboard/data/repositories/dashboard_repository_impl.dart';
import 'features/dashboard/domain/usecases/get_dashboard_summary_usecase.dart';
import 'features/dashboard/domain/usecases/toggle_auto_watering_usecase.dart';
import 'features/dashboard/presentation/bloc/dashboard_bloc.dart';

import 'features/automation/domain/repositories/i_automation_repository.dart';
import 'features/automation/data/repositories/automation_repository_impl.dart';
import 'features/automation/presentation/bloc/automation_bloc.dart';

import 'features/sensor_monitor/domain/repositories/sensor_repository.dart';
import 'features/sensor_monitor/domain/usecases/get_realtime_readings_usecase.dart';
import 'features/sensor_monitor/domain/usecases/get_sensor_history_usecase.dart';
import 'features/sensor_monitor/presentation/bloc/sensor_bloc.dart';

import 'features/device_management/domain/repositories/device_repository.dart';
import 'features/device_management/domain/usecases/get_devices_usecase.dart';
import 'features/device_management/domain/usecases/add_device_usecase.dart';
import 'features/device_management/domain/usecases/delete_device_usecase.dart';
import 'features/device_management/presentation/bloc/device_bloc.dart';

final getIt = GetIt.instance;

Future<void> configureDependencies() async {
  // Repositories (Mocked for UI-only testing)
  getIt.registerLazySingleton<AuthRepository>(() => MockAuthRepository());
  getIt.registerLazySingleton<DashboardRepository>(() => DashboardRepositoryImpl());
  getIt.registerLazySingleton<SensorRepository>(() => MockSensorRepository());
  getIt.registerLazySingleton<DeviceRepository>(() => MockDeviceRepository());
  getIt.registerLazySingleton<IAutomationRepository>(() => AutomationRepositoryImpl());

  // UseCases - Auth
  getIt.registerLazySingleton(() => LoginUseCase(getIt<AuthRepository>()));
  getIt.registerLazySingleton(() => LogoutUseCase(getIt<AuthRepository>()));
  getIt.registerLazySingleton(() => GetCurrentUserUseCase(getIt<AuthRepository>()));

  // UseCases - Dashboard
  getIt.registerLazySingleton(() => GetDashboardSummaryUseCase(getIt<DashboardRepository>()));
  getIt.registerLazySingleton(() => ToggleAutoWateringUseCase(getIt<DashboardRepository>()));

  // UseCases - Sensor
  getIt.registerLazySingleton(() => GetSensorHistoryUseCase(getIt<SensorRepository>()));
  getIt.registerLazySingleton(() => GetRealtimeReadingsUseCase(getIt<SensorRepository>()));

  // UseCases - Device
  getIt.registerLazySingleton(() => GetDevicesUseCase(getIt<DeviceRepository>()));
  getIt.registerLazySingleton(() => AddDeviceUseCase(getIt<DeviceRepository>()));
  getIt.registerLazySingleton(() => DeleteDeviceUseCase(getIt<DeviceRepository>()));

  // BLoCs
  getIt.registerFactory(() => AuthBloc(
    loginUseCase: getIt<LoginUseCase>(),
    logoutUseCase: getIt<LogoutUseCase>(),
    getCurrentUserUseCase: getIt<GetCurrentUserUseCase>(),
  ));

  getIt.registerFactory(() => DashboardBloc(
    repository: getIt<DashboardRepository>(),
  ));

  getIt.registerFactory(() => AutomationBloc(
    repository: getIt<IAutomationRepository>(),
  ));

  getIt.registerFactory(() => SensorBloc(
    getSensorHistory: getIt<GetSensorHistoryUseCase>(),
    getRealtimeReadings: getIt<GetRealtimeReadingsUseCase>(),
  ));

  getIt.registerFactory(() => DeviceBloc(
    getDevices: getIt<GetDevicesUseCase>(),
    addDevice: getIt<AddDeviceUseCase>(),
    deleteDevice: getIt<DeleteDeviceUseCase>(),
    deviceRepository: getIt<DeviceRepository>(),
  ));
}
