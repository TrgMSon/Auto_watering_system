import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import '../../../core/constants/api_endpoints.dart';
import '../../../core/network/api_client.dart';

/// Lấy `ble_secret_key` từ server và lưu trên điện thoại để dùng khi không có mạng.
class BleKeyRepository {
  BleKeyRepository(this._apiClient, {FlutterSecureStorage? storage})
      : _storage = storage ?? const FlutterSecureStorage();

  static const String _storagePrefix = 'ble_secret_key_';

  final ApiClient _apiClient;
  final FlutterSecureStorage _storage;

  /// Trả về khóa mới nhất, hoặc khóa đã lưu nếu không gọi được server.
  /// Trả về null nếu chưa từng có khóa hoặc người dùng không còn quyền với thiết bị.
  Future<String?> getKey(String deviceId) async {
    try {
      return await _fetchAndStore(deviceId);
    } on ApiException catch (e) {
      if (_isAccessRevoked(e)) {
        await _storage.delete(key: _storageKey(deviceId));
        return null;
      }
      if (_isServerUnreachable(e)) {
        return _storage.read(key: _storageKey(deviceId));
      }
      rethrow;
    }
  }

  Future<String> _fetchAndStore(String deviceId) async {
    final path = '${ApiEndpoints.devices}/${Uri.encodeComponent(deviceId)}/ble-secret-key';
    final response = await _apiClient.get(path);
    final key = response['ble_secret_key'];
    if (key is! String || key.isEmpty) {
      throw const ApiException(200, 'Phản hồi không có khóa BLE.');
    }
    await _storage.write(key: _storageKey(deviceId), value: key);
    return key;
  }

  String _storageKey(String deviceId) => '$_storagePrefix$deviceId';

  /// Backend trả 404 cả khi viewer bị khóa, thiết bị đã gỡ hoặc đã ngừng dùng.
  bool _isAccessRevoked(ApiException e) => e.statusCode == 403 || e.statusCode == 404;

  bool _isServerUnreachable(ApiException e) =>
      e.statusCode == 0 || e.statusCode == 408 || e.statusCode >= 500;
}
