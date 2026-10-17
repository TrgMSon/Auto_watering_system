import '../entities/device.dart';

abstract class DeviceRepository {
  Future<List<Device>> getDevices();
  Future<Device> getDeviceById(int id);
  Future<void> addDevice({required String name, required String type, String? location});
  Future<void> updateDevice({required int id, String? name, String? location});
  Future<void> deleteDevice(int id);
  Future<bool> toggleDevice({required int id, required bool activate});
}
