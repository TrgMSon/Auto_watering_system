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
