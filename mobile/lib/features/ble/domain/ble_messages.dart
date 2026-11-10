import 'dart:convert';

import 'package:equatable/equatable.dart';

import 'ble_protocol.dart';

Map<String, dynamic> _decodeObject(List<int> bytes) {
  final decoded = jsonDecode(utf8.decode(bytes));
  if (decoded is! Map<String, dynamic>) {
    throw const FormatException('Gói BLE không phải JSON object');
  }
  return decoded;
}

/// Gói cảm biến từ `0001`. Giá trị null nghĩa là cảm biến đó đang lỗi, không phải bằng 0.
class BleSensorSample extends Equatable {
  const BleSensorSample({
    required this.soilMoisture,
    required this.temperature,
    required this.humidity,
    required this.waterLevelPercent,
    required this.isPumpOn,
    this.isAutoMode,
  });

  /// Ném [FormatException] nếu dữ liệu sai định dạng.
  factory BleSensorSample.fromBytes(List<int> bytes) {
    final json = _decodeObject(bytes);
    final pump = json['pump'];
    if (pump != 0 && pump != 1) {
      throw const FormatException('Trường pump phải là 0 hoặc 1');
    }
    final auto = json['auto'];
    if (auto != null && auto != 0 && auto != 1) {
      throw const FormatException('Trường auto phải là 0, 1 hoặc null');
    }
    return BleSensorSample(
      soilMoisture: _optionalNumber(json, 'soil'),
      temperature: _optionalNumber(json, 'temp'),
      humidity: _optionalNumber(json, 'hum'),
      waterLevelPercent: _optionalNumber(json, 'water'),
      isPumpOn: pump == 1,
      isAutoMode: auto == null ? null : auto == 1,
    );
  }

  final double? soilMoisture;
  final double? temperature;
  final double? humidity;

  /// Mực nước theo % bồn (0 đến 100), từ trường `water` của firmware.
  final double? waterLevelPercent;
  final bool isPumpOn;
  final bool? isAutoMode;

  static double? _optionalNumber(Map<String, dynamic> json, String key) {
    final value = json[key];
    if (value == null) return null;
    if (value is num) return value.toDouble();
    throw FormatException('Trường $key phải là số hoặc null');
  }

  @override
  List<Object?> get props => [
        soilMoisture,
        temperature,
        humidity,
        waterLevelPercent,
        isPumpOn,
        isAutoMode
      ];
}

/// Kết quả nghiệp vụ từ `0005`, ví dụ `{"op":"PUMP_ON","code":"OK"}`.
class BleCommandResult extends Equatable {
  const BleCommandResult({required this.op, required this.code});

  /// Ném [FormatException] nếu dữ liệu sai định dạng.
  factory BleCommandResult.fromBytes(List<int> bytes) {
    final json = _decodeObject(bytes);
    final op = json['op'];
    final code = json['code'];
    if (op is! String || code is! String) {
      throw const FormatException('Kết quả BLE thiếu op hoặc code');
    }
    return BleCommandResult(op: op, code: BleResultCode.fromWire(code));
  }

  final String op;
  final BleResultCode code;

  bool get isSuccess => code == BleResultCode.ok;

  @override
  List<Object?> get props => [op, code];
}
