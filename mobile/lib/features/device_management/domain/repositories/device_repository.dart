import '../entities/device.dart';

abstract class DeviceRepository {
  Future<List<Device>> getDevices();
  Future<Device> getDeviceById(String id);
  Future<void> addDevice({required String name, required String type, String? location});
  Future<void> updateDevice({required String id, String? name, String? location});
  Future<void> deleteDevice(String id);
  Future<bool> toggleDevice({required String id, required bool activate});
}
