/// Hằng số và kiểu dữ liệu theo hợp đồng firmware/docs/hop-dong-BLE-chot-2026-11-04.md.
class BleUuids {
  BleUuids._();

  static const String _prefix = '07975e87-679e-531b-b48b-d227f457';

  static const String service = '${_prefix}0000';
  static const String sensor = '${_prefix}0001';
  static const String pump = '${_prefix}0002';
  static const String config = '${_prefix}0003';
  static const String nonce = '${_prefix}0004';
  static const String result = '${_prefix}0005';
  static const String wifi = '${_prefix}0006';
  static const String time = '${_prefix}0007';
}

/// Mã kết quả ESP32 gửi qua `0005`.
enum BleResultCode {
  ok('OK'),
  badSig('BAD_SIG'),
  nonceExpired('NONCE_EXPIRED'),
  badJson('BAD_JSON'),
  outOfRange('OUT_OF_RANGE'),
  lowWater('LOW_WATER'),
  maintenance('MAINTENANCE'),
  sensorError('SENSOR_ERROR'),
  pumpBusy('PUMP_BUSY'),
  invalidTime('INVALID_TIME'),
  wifiOk('WIFI_OK'),
  wifiFail('WIFI_FAIL'),
  storageError('STORAGE_ERROR'),

  /// Firmware gửi mã ngoài hợp đồng; giữ lại để app báo lỗi thay vì chờ hết giờ.
  unknown('UNKNOWN');

  const BleResultCode(this.wire);

  final String wire;

  static BleResultCode fromWire(String value) => BleResultCode.values.firstWhere(
        (code) => code.wire == value,
        orElse: () => BleResultCode.unknown,
      );
}

/// Chuyển đổi giữa tên BLE `WS-383E516FC7A8` và mã thiết bị `38:3E:51:6F:C7:A8`.
class BleDeviceId {
  BleDeviceId._();

  static const String _namePrefix = 'WS-';
  static final RegExp _hex12 = RegExp(r'^[0-9A-F]{12}$');

  /// Trả về null nếu tên không đúng định dạng của thiết bị trong hệ thống.
  static String? fromAdvertisingName(String name) {
    if (!name.startsWith(_namePrefix)) return null;
    final hex = name.substring(_namePrefix.length);
    if (!_hex12.hasMatch(hex)) return null;
    final pairs = [
      for (var i = 0; i < hex.length; i += 2) hex.substring(i, i + 2),
    ];
    return pairs.join(':');
  }

  static String toAdvertisingName(String deviceId) =>
      '$_namePrefix${deviceId.replaceAll(':', '').toUpperCase()}';
}
