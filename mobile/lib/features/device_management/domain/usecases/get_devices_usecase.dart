import '../entities/device.dart';
import '../repositories/device_repository.dart';

class GetDevicesUseCase {
  final DeviceRepository repository;
  const GetDevicesUseCase(this.repository);
  Future<List<Device>> call() => repository.getDevices();
}
