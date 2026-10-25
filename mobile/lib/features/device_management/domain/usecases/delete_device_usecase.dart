import '../repositories/device_repository.dart';

class DeleteDeviceUseCase {
  final DeviceRepository repository;
  const DeleteDeviceUseCase(this.repository);
  Future<void> call(String id) => repository.deleteDevice(id);
}
