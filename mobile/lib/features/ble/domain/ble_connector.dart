import 'ble_transport.dart';

/// Lỗi khi quét, kết nối hoặc ghép nối; [message] hiện thẳng cho người dùng.
class BleConnectException implements Exception {
  const BleConnectException(this.message);

  final String message;

  @override
  String toString() => message;
}

/// Một kết nối tới đúng ESP32 đã chọn.
abstract interface class BleLink {
  BleTransport get transport;

  /// Hoàn thành khi mất kết nối, kể cả do ESP32 tắt hoặc ra khỏi tầm.
  Future<void> get disconnected;

  /// Ghép nối (bond) để gửi Wi-Fi. Firmware chỉ nhận ghép nối khi cửa sổ ghép nối đang mở.
  Future<void> ensureBonded();

  Future<void> disconnect();
}

abstract interface class BleConnector {
  /// Quét tìm ESP32 có tên `WS-<MAC>` ứng với [deviceId] rồi kết nối.
  /// Ném [BleConnectException] khi Bluetooth tắt, không tìm thấy hoặc kết nối lỗi.
  Future<BleLink> connect(String deviceId);
}
