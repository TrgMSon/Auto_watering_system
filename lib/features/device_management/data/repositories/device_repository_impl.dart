import '../../domain/entities/device.dart';
import '../../domain/repositories/device_repository.dart';
import '../datasources/device_remote_datasource.dart';

class DeviceRepositoryImpl implements DeviceRepository {
  final DeviceRemoteDataSource remoteDataSource;
  const DeviceRepositoryImpl({required this.remoteDataSource});

  @override
  Future<List<Device>> getDevices() => remoteDataSource.getDevices();

  @override
  Future<Device> getDeviceById(int id) => remoteDataSource.getDeviceById(id);

  @override
  Future<void> addDevice({required String name, required String type, String? location}) =>
      remoteDataSource.addDevice(name: name, type: type, location: location);

  @override
  Future<void> updateDevice({required int id, String? name, String? location}) =>
      remoteDataSource.updateDevice(id: id, name: name, location: location);

  @override
  Future<void> deleteDevice(int id) => remoteDataSource.deleteDevice(id);

  @override
  Future<bool> toggleDevice({required int id, required bool activate}) =>
      remoteDataSource.toggleDevice(id: id, activate: activate);
}
