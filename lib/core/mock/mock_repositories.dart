import 'dart:async';
import 'dart:math';

import '../../features/auth/domain/entities/user.dart';
import '../../features/auth/domain/repositories/auth_repository.dart';
import '../../features/dashboard/domain/entities/dashboard_summary.dart';
import '../../features/dashboard/domain/repositories/dashboard_repository.dart';
import '../../features/sensor_monitor/domain/entities/sensor_reading.dart';
import '../../features/sensor_monitor/domain/repositories/sensor_repository.dart';
import '../../features/device_management/domain/entities/device.dart';
import '../../features/device_management/domain/repositories/device_repository.dart';

class MockAuthRepository implements AuthRepository {
  bool _isLoggedIn = false;
  User? _currentUser;

  @override
  Future<User> login({required String username, required String password}) async {
    await Future.delayed(const Duration(seconds: 1));
    _isLoggedIn = true;
    _currentUser = User(
      id: 1,
      username: username,
      email: '\$username@example.com',
      fullName: 'Người dùng Thử nghiệm',
      role: UserRole.admin,
    );
    return _currentUser!;
  }

  @override
  Future<void> logout() async {
    await Future.delayed(const Duration(milliseconds: 500));
    _isLoggedIn = false;
    _currentUser = null;
  }

  @override
  Future<User?> getCurrentUser() async {
    await Future.delayed(const Duration(milliseconds: 500));
    return _currentUser;
  }

  @override
  Future<bool> isLoggedIn() async {
    return _isLoggedIn;
  }
}

class MockDashboardRepository implements DashboardRepository {
  bool _isAutoWatering = false;

  @override
  Future<DashboardSummary> getDashboardSummary() async {
    await Future.delayed(const Duration(seconds: 1));
    return DashboardSummary(
      isAutoWateringEnabled: _isAutoWatering,
      soilMoisture: 45.5,
      temperature: 28.2,
      humidity: 65.0,
      lightIntensity: 1200,
      totalDevices: 5,
      onlineDevices: 4,
      lastWateredAt: DateTime.now().subtract(const Duration(hours: 2)),
    );
  }

  @override
  Future<bool> toggleAutoWatering({required bool enabled}) async {
    await Future.delayed(const Duration(milliseconds: 500));
    _isAutoWatering = enabled;
    return true;
  }
}

class MockSensorRepository implements SensorRepository {
  final _random = Random();
  final _controller = StreamController<SensorReading>.broadcast();
  Timer? _timer;

  @override
  Future<List<SensorReading>> getSensorHistory({required DateTime from, required DateTime to}) async {
    await Future.delayed(const Duration(seconds: 1));
    final list = <SensorReading>[];
    var current = from;
    while (current.isBefore(to)) {
      list.add(SensorReading(
        id: current.millisecondsSinceEpoch,
        soilMoisture: 30 + _random.nextDouble() * 40,
        temperature: 20 + _random.nextDouble() * 15,
        humidity: 50 + _random.nextDouble() * 30,
        lightIntensity: 500 + _random.nextDouble() * 2000,
        timestamp: current,
      ));
      current = current.add(const Duration(hours: 1));
    }
    return list;
  }

  @override
  Stream<SensorReading> getRealtimeReadings() {
    _timer?.cancel();
    _timer = Timer.periodic(const Duration(seconds: 3), (timer) {
      if (!_controller.isClosed) {
        _controller.add(SensorReading(
          id: DateTime.now().millisecondsSinceEpoch,
          soilMoisture: 40 + _random.nextDouble() * 10,
          temperature: 25 + _random.nextDouble() * 5,
          humidity: 60 + _random.nextDouble() * 10,
          lightIntensity: 1000 + _random.nextDouble() * 500,
          timestamp: DateTime.now(),
        ));
      }
    });
    return _controller.stream;
  }
}

class MockDeviceRepository implements DeviceRepository {
  final List<Device> _devices = [
    Device(id: 1, name: 'Máy bơm khu A', type: DeviceType.pump, isOnline: true, isActive: false, location: 'Vườn trước'),
    Device(id: 2, name: 'Van nước khu B', type: DeviceType.valve, isOnline: true, isActive: true, location: 'Vườn sau'),
    Device(id: 3, name: 'Cảm biến đất 1', type: DeviceType.sensor, isOnline: true, location: 'Gốc cây táo'),
    Device(id: 4, name: 'Gateway', type: DeviceType.gateway, isOnline: false, location: 'Phòng khách'),
  ];

  @override
  Future<List<Device>> getDevices() async {
    await Future.delayed(const Duration(seconds: 1));
    return List.from(_devices);
  }

  @override
  Future<Device> getDeviceById(int id) async {
    return _devices.firstWhere((d) => d.id == id);
  }

  @override
  Future<void> addDevice({required String name, required String type, String? location}) async {
    await Future.delayed(const Duration(milliseconds: 500));
    final newDevice = Device(
      id: DateTime.now().millisecondsSinceEpoch,
      name: name,
      type: type == 'PUMP' ? DeviceType.pump : type == 'VALVE' ? DeviceType.valve : type == 'GATEWAY' ? DeviceType.gateway : DeviceType.sensor,
      isOnline: true,
      location: location,
    );
    _devices.add(newDevice);
  }

  @override
  Future<void> updateDevice({required int id, String? name, String? location}) async {}

  @override
  Future<void> deleteDevice(int id) async {
    await Future.delayed(const Duration(milliseconds: 500));
    _devices.removeWhere((d) => d.id == id);
  }

  @override
  Future<bool> toggleDevice({required int id, required bool activate}) async {
    await Future.delayed(const Duration(milliseconds: 300));
    final index = _devices.indexWhere((d) => d.id == id);
    if (index >= 0) {
      final old = _devices[index];
      _devices[index] = Device(
        id: old.id, name: old.name, type: old.type, isOnline: old.isOnline, isActive: activate, location: old.location, lastSeen: old.lastSeen,
      );
      return true;
    }
    return false;
  }
}
