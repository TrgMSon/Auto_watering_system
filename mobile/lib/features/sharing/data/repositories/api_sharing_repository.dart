import '../../../../core/constants/api_endpoints.dart';
import '../../../../core/network/api_client.dart';
import '../../domain/entities/device_share.dart';
import '../../domain/repositories/sharing_repository.dart';

class ApiSharingRepository implements SharingRepository {
  ApiSharingRepository(this._apiClient);

  final ApiClient _apiClient;

  String _viewersPath(String deviceId) =>
      '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/viewers';

  @override
  Future<List<DeviceShare>> getOwnedDeviceShares() async {
    final response = await _apiClient.get(ApiEndpoints.devices);
    final rows = response['data'];
    if (rows is! List) return const [];
    final ownedDevices = rows
        .whereType<Map<String, dynamic>>()
        .where((json) => json['permission']?.toString().toLowerCase() == 'owner');
    return Future.wait(ownedDevices.map(_loadShare));
  }

  Future<DeviceShare> _loadShare(Map<String, dynamic> device) async {
    final deviceId = device['id']?.toString() ?? '';
    final response = await _apiClient.get(_viewersPath(deviceId));
    final rows = response['data'];
    final name = device['device_name']?.toString() ?? '';
    return DeviceShare(
      deviceId: deviceId,
      deviceName: name.isEmpty ? deviceId : name,
      viewers: rows is List
          ? rows.whereType<Map<String, dynamic>>().map(_viewerFromJson).toList()
          : const [],
    );
  }

  SharedViewer _viewerFromJson(Map<String, dynamic> json) {
    return SharedViewer(
      id: (json['viewerId'] as num).toInt(),
      email: json['email']?.toString() ?? '',
      isLocked: json['permission']?.toString().toLowerCase() == 'viewer_locked',
    );
  }

  @override
  Future<void> addViewer(String deviceId, String email) async {
    await _apiClient.post(_viewersPath(deviceId), body: {'email': email});
  }

  @override
  Future<void> removeViewer(String deviceId, int viewerId) {
    return _apiClient.delete('${_viewersPath(deviceId)}/$viewerId');
  }

  @override
  Future<void> setViewerLocked(
    String deviceId,
    int viewerId, {
    required bool locked,
  }) async {
    final path = '${_viewersPath(deviceId)}/$viewerId/lock';
    if (locked) {
      await _apiClient.post(path);
    } else {
      await _apiClient.delete(path);
    }
  }
}
