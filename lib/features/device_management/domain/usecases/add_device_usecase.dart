import '../repositories/device_repository.dart';

class AddDeviceUseCase {
  final DeviceRepository repository;
  const AddDeviceUseCase(this.repository);

  Future<void> call({
    required String name,
    required String type,
    String? location,
  }) => repository.addDevice(name: name, type: type, location: location);
}
