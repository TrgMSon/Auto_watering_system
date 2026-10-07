import '../repositories/device_repository.dart';

class DeleteDeviceUseCase {
  final DeviceRepository repository;
  const DeleteDeviceUseCase(this.repository);
  Future<void> call(int id) => repository.deleteDevice(id);
}
