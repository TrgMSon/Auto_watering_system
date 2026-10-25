import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/entities/device.dart';
import '../../domain/repositories/device_repository.dart';

class ApiDeviceRepository implements DeviceRepository {
  ApiDeviceRepository(this._apiClient);

  final ApiClient _apiClient;

  @override
  Future<List<Device>> getDevices() async {
    final response = await _apiClient.get(ApiEndpoints.devices);
    final rows = response['data'];
    if (rows is! List) return const [];
    return rows.whereType<Map<String, dynamic>>().map((json) {
      final status = json['status']?.toString().toUpperCase();
      return Device(
        id: json['id']?.toString() ?? '',
        name: json['device_name']?.toString() ?? 'Thiết bị',
        type: DeviceType.unknown,
        isOnline: status == 'ONLINE',
        location: null,
      );
    }).toList();
  }

  @override
  Future<Device> getDeviceById(String id) async {
    final devices = await getDevices();
    return devices.firstWhere(
      (device) => device.id == id,
      orElse: () => throw ApiException(404, 'Không tìm thấy thiết bị.'),
    );
  }

  @override
  Future<void> addDevice({
    required String name,
    required String type,
    String? location,
  }) =>
      Future<void>.error(const ApiException(
        501,
        'Backend chưa hỗ trợ đăng ký thiết bị từ ứng dụng.',
      ));

  @override
  Future<void> updateDevice({
    required String id,
    String? name,
    String? location,
  }) =>
      Future<void>.error(const ApiException(
        501,
        'Backend chưa hỗ trợ chỉnh sửa thông tin thiết bị.',
      ));

  @override
  Future<void> deleteDevice(String id) => Future<void>.error(const ApiException(
        501,
        'Backend chưa hỗ trợ hủy liên kết thiết bị.',
      ));

  @override
  Future<bool> toggleDevice({required String id, required bool activate}) =>
      Future<bool>.error(const ApiException(
        501,
        'Hãy dùng Dashboard để gửi lệnh bật/tắt máy bơm.',
      ));
}
