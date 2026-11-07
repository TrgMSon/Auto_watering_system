import 'dart:convert';

import '../../automation/data/models/device_config_model.dart';
import 'ble_protocol.dart';
import 'ble_signer.dart';

/// Lệnh app ghi xuống ESP32. Mỗi lệnh cần một nonce mới đọc từ `0004`.
sealed class BleCommand {
  const BleCommand();

  /// Tên `op` ESP32 dùng khi trả kết quả qua `0005`.
  String get op;

  String get characteristicUuid;

  String signingMessage(String nonce);

  Map<String, Object> payloadFields();

  /// Thời gian chờ kết quả riêng của lệnh; null thì dùng mặc định 3 giây.
  Duration? get resultTimeout => null;

  List<int> encode({required String nonce, required String secretKey}) {
    final sig = BleSigner.sign(secretKey, signingMessage(nonce));
    return utf8.encode(jsonEncode({...payloadFields(), 'sig': sig}));
  }
}

class BlePumpOnCommand extends BleCommand {
  /// [durationSeconds] không được vượt `max_dur` đang lưu; ESP32 kiểm tra lần cuối.
  BlePumpOnCommand(this.durationSeconds) {
    if (durationSeconds < 1 || durationSeconds > BleConfigCommand.maxDurationLimit) {
      throw ArgumentError.value(durationSeconds, 'durationSeconds', 'Phải từ 1 đến 600 giây');
    }
  }

  final int durationSeconds;

  @override
  String get op => 'PUMP_ON';

  @override
  String get characteristicUuid => BleUuids.pump;

  @override
  String signingMessage(String nonce) => '$nonce|PUMP_ON|$durationSeconds';

  @override
  Map<String, Object> payloadFields() => {'cmd': 'PUMP_ON', 'dur': durationSeconds};
}

class BlePumpOffCommand extends BleCommand {
  const BlePumpOffCommand();

  @override
  String get op => 'PUMP_OFF';

  @override
  String get characteristicUuid => BleUuids.pump;

  @override
  String signingMessage(String nonce) => '$nonce|PUMP_OFF|0';

  @override
  Map<String, Object> payloadFields() => {'cmd': 'PUMP_OFF', 'dur': 0};
}

/// Firmware hiện tắt `0003` (ghi bị từ chối) và lưu ngưỡng nước theo % chứ không theo cm,
/// nên chưa dùng lệnh này cho đến khi nhóm chốt lại cấu hình qua BLE.
class BleConfigCommand extends BleCommand {
  /// Làm tròn trước khi kiểm tra để chữ ký và JSON luôn dùng cùng một giá trị một chữ số thập phân.
  BleConfigCommand({
    required double soilMin,
    required double soilMax,
    required this.autoMode,
    required this.maxDurationSeconds,
    required double minWaterCm,
  })  : soilMin = _oneDecimal(soilMin),
        soilMax = _oneDecimal(soilMax),
        minWaterCm = _oneDecimal(minWaterCm) {
    if (this.soilMin < 0 || this.soilMin >= this.soilMax || this.soilMax > 100) {
      throw ArgumentError('Cần 0 <= min < max <= 100');
    }
    if (maxDurationSeconds < minDurationLimit || maxDurationSeconds > maxDurationLimit) {
      throw ArgumentError.value(maxDurationSeconds, 'maxDurationSeconds', 'Phải từ 60 đến 600 giây');
    }
    if (this.minWaterCm < 0) {
      throw ArgumentError.value(minWaterCm, 'minWaterCm', 'Không được âm');
    }
  }

  /// App và backend lưu thời gian theo phút, BLE gửi theo giây.
  factory BleConfigCommand.fromDeviceConfig(DeviceConfigModel config) => BleConfigCommand(
        soilMin: config.soilMoistureMin,
        soilMax: config.soilMoistureMax,
        autoMode: config.autoMode,
        maxDurationSeconds: config.maxWateringDurationSeconds,
        minWaterCm: config.waterLevelMin.toDouble(),
      );

  static const int minDurationLimit = 60;
  static const int maxDurationLimit = 600;

  final double soilMin;
  final double soilMax;
  final bool autoMode;
  final int maxDurationSeconds;
  final double minWaterCm;

  int get _autoFlag => autoMode ? 1 : 0;

  @override
  String get op => 'CONFIG';

  @override
  String get characteristicUuid => BleUuids.config;

  @override
  String signingMessage(String nonce) => [
        nonce,
        'CONFIG',
        soilMin.toStringAsFixed(1),
        soilMax.toStringAsFixed(1),
        _autoFlag,
        maxDurationSeconds,
        minWaterCm.toStringAsFixed(1),
      ].join('|');

  @override
  Map<String, Object> payloadFields() => {
        'min': soilMin,
        'max': soilMax,
        'auto': _autoFlag,
        'max_dur': maxDurationSeconds,
        'min_water': minWaterCm,
      };

  static double _oneDecimal(double value) => double.parse(value.toStringAsFixed(1));
}

/// Gửi tên và mật khẩu Wi-Fi qua `0006`. Firmware chỉ nhận khi điện thoại đã ghép nối (bond)
/// với ESP32 và liên kết đã mã hóa.
class BleWifiCommand extends BleCommand {
  BleWifiCommand({required this.ssid, required this.password}) {
    final ssidBytes = utf8.encode(ssid).length;
    final passwordBytes = utf8.encode(password).length;
    if (ssidBytes < 1 || ssidBytes > 32) {
      throw ArgumentError.value(ssid, 'ssid', 'Tên Wi-Fi phải từ 1 đến 32 byte');
    }
    // Firmware chỉ hỗ trợ WPA2 có mật khẩu, không nhận mạng không mật khẩu.
    if (passwordBytes < 8 || passwordBytes > 63) {
      throw ArgumentError('Mật khẩu Wi-Fi phải từ 8 đến 63 byte');
    }
    // Firmware từ chối dấu `|` vì chuỗi ký dùng dấu này để ngăn cách.
    if (ssid.contains('|') || password.contains('|')) {
      throw ArgumentError('Tên và mật khẩu Wi-Fi không được chứa dấu |');
    }
  }

  /// Firmware thử kết nối tối đa 30 giây; cộng thêm thời gian gửi kết quả qua BLE.
  static const Duration timeout = Duration(seconds: 35);

  final String ssid;
  final String password;

  @override
  String get op => 'WIFI';

  @override
  String get characteristicUuid => BleUuids.wifi;

  @override
  String signingMessage(String nonce) => '$nonce|WIFI|$ssid|$password';

  @override
  Map<String, Object> payloadFields() => {'ssid': ssid, 'pass': password};

  @override
  Duration get resultTimeout => timeout;
}

class BleTimeSyncCommand extends BleCommand {
  BleTimeSyncCommand(this.unixSeconds) {
    if (unixSeconds <= 0) {
      throw ArgumentError.value(unixSeconds, 'unixSeconds', 'Phải là giờ UTC hợp lệ');
    }
  }

  factory BleTimeSyncCommand.fromDateTime(DateTime time) =>
      BleTimeSyncCommand(time.toUtc().millisecondsSinceEpoch ~/ 1000);

  final int unixSeconds;

  @override
  String get op => 'TIME';

  @override
  String get characteristicUuid => BleUuids.time;

  @override
  String signingMessage(String nonce) => '$nonce|TIME|$unixSeconds';

  @override
  Map<String, Object> payloadFields() => {'unix': unixSeconds};
}
