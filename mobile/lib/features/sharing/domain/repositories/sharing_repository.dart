import '../entities/device_share.dart';

abstract class SharingRepository {
  Future<List<DeviceShare>> getOwnedDeviceShares();
  Future<void> addViewer(String deviceId, String email);
  Future<void> removeViewer(String deviceId, int viewerId);
  Future<void> setViewerLocked(String deviceId, int viewerId, {required bool locked});
}
