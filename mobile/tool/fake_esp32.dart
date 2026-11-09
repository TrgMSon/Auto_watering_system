import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:crypto/crypto.dart';
import 'package:watering_app/features/ble/domain/ble_command_service.dart';
import 'package:watering_app/features/ble/domain/ble_protocol.dart';
import 'package:watering_app/features/ble/domain/ble_transport.dart';

/// ESP32 giả để chạy thử luồng BLE, làm theo firmware nhánh `main` (08/11).
/// Cố ý tự kiểm tra chữ ký bằng code riêng, để lỗi ở phía app không bị che khuất.
class FakeEsp32 implements BleTransport {
  FakeEsp32({
    required this.secretKey,
    DateTime Function()? now,
    this.maxWritePayloadBytes = 182,
  }) : _now = now ?? DateTime.now;

  static const Duration nonceLifetime = Duration(seconds: 30);

  /// Firmware chỉ cho bơm tay tối đa 5 giây.
  static const int qualificationSeconds = 5;

  // Khoảng giờ firmware chấp nhận: 2024-01-01 đến 2099-12-31 UTC.
  static const int _unixMin = 1704067200;
  static const int _unixMax = 4102444799;

  final String secretKey;
  final DateTime Function() _now;
  final Random _random = Random();
  final StreamController<List<int>> _results = StreamController.broadcast();
  final StreamController<List<int>> _sensors = StreamController.broadcast();

  @override
  final int maxWritePayloadBytes;

  String? _nonce;
  DateTime? _nonceIssuedAt;

  bool isPumpOn = false;
  bool isAutoMode = false;

  /// Đang chờ giữa hai lần tưới hoặc bị khóa: firmware không có mã kết quả cho hai trạng thái này.
  bool isBlockedByController = false;
  int maxRunSeconds = 120;
  double minWaterPercent = 20;

  /// Mực nước theo % bồn, gửi trong trường `water` của `0001`.
  double? waterPercent = 60;
  double soilMin = 30;
  double soilMax = 70;
  bool isSoilValid = true;

  /// Điện thoại đã ghép nối và liên kết đã mã hóa; bắt buộc với `0006`.
  bool isBonded = true;
  bool wifiConnects = true;
  String? savedSsid;
  int? syncedUnixSeconds;

  /// Bật để giả lập ESP32 nhận lệnh nhưng không trả kết quả.
  bool dropResults = false;
  int writeCount = 0;

  @override
  Future<List<int>> read(String characteristicUuid) async {
    return switch (characteristicUuid) {
      BleUuids.nonce => utf8.encode(_issueNonce()),
      BleUuids.sensor => _sensorPacket(),
      _ => throw UnsupportedError('Không đọc được $characteristicUuid'),
    };
  }

  @override
  Future<Stream<List<int>>> subscribe(String characteristicUuid) async {
    return switch (characteristicUuid) {
      BleUuids.result => _results.stream,
      BleUuids.sensor => _sensors.stream,
      _ => throw UnsupportedError('Không có Notify trên $characteristicUuid'),
    };
  }

  @override
  Future<void> write(String characteristicUuid, List<int> value) async {
    writeCount++;
    if (value.length > maxWritePayloadBytes) {
      _consumeNonce();
      throw const BleWriteRejectedException('Gói vượt MTU');
    }
    final (op, code) = switch (characteristicUuid) {
      BleUuids.pump => _handlePump(value),
      BleUuids.time => ('TIME', _handleTime(value)),
      BleUuids.wifi => ('WIFI', _handleWifi(value)),
      BleUuids.config => ('CONFIG', _handleConfig(value)),
      _ => throw UnsupportedError('Không ghi được $characteristicUuid'),
    };
    // Firmware trả lỗi ATT khi kết quả không có mã trong hợp đồng.
    if (code == null) throw const BleWriteRejectedException();
    if (!dropResults) emitResult(op, code);
  }

  void emitResult(String op, String code) {
    _results.add(utf8.encode(jsonEncode({'op': op, 'code': code})));
  }

  void emitRawResult(List<int> packet) => _results.add(packet);

  void emitSensorPacket() => _sensors.add(_sensorPacket());

  void emitRawSensorPacket(List<int> packet) => _sensors.add(packet);

  Future<void> disconnect() async {
    await _results.close();
    await _sensors.close();
  }

  String _issueNonce() {
    final bytes = List.generate(8, (_) => _random.nextInt(256));
    _nonce = bytes.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
    _nonceIssuedAt = _now();
    return _nonce!;
  }

  List<int> _sensorPacket() => utf8.encode(jsonEncode({
        'soil': isSoilValid ? 42.5 : null,
        'temp': 31.8,
        'hum': 68.0,
        'water': waterPercent,
        'pump': isPumpOn ? 1 : 0,
      }));

  /// Theo thứ tự kiểm tra của `System::startManual` trong firmware.
  (String, String?) _handlePump(List<int> value) {
    final json = _decode(value);
    final cmd = json?['cmd'];
    final dur = json?['dur'];
    final sig = json?['sig'];
    if (cmd is! String || dur is! int || sig is! String) return ('PUMP', 'BAD_JSON');
    if (cmd != 'PUMP_ON' && cmd != 'PUMP_OFF') return (cmd, 'BAD_JSON');
    final isRangeValid = cmd == 'PUMP_OFF' ? dur == 0 : dur >= 1 && dur <= 600;

    final authError = _authenticate(sig, (nonce) => '$nonce|$cmd|$dur');
    if (authError != null) return (cmd, authError);
    if (!isRangeValid) return (cmd, 'OUT_OF_RANGE');

    if (cmd == 'PUMP_OFF') {
      isPumpOn = false;
      isAutoMode = false;
      return (cmd, 'OK');
    }
    if (isAutoMode) return (cmd, null);
    if (isPumpOn) return (cmd, 'PUMP_BUSY');
    if (dur > maxRunSeconds || dur > qualificationSeconds) return (cmd, 'OUT_OF_RANGE');
    if (isBlockedByController) return (cmd, null);
    final water = waterPercent;
    if (!isSoilValid || water == null) return (cmd, 'SENSOR_ERROR');
    if (water < minWaterPercent) return (cmd, 'LOW_WATER');
    isPumpOn = true;
    return (cmd, 'OK');
  }

  /// Theo `ble_commands::execute` với lệnh CONFIG trong firmware.
  String _handleConfig(List<int> value) {
    final json = _decode(value);
    final min = json?['min'];
    final max = json?['max'];
    final auto = json?['auto'];
    final maxDur = json?['max_dur'];
    final minWater = json?['min_water'];
    final sig = json?['sig'];
    if (min is! double || max is! double || auto is! int || maxDur is! int) return 'BAD_JSON';
    if (minWater is! double || sig is! String) return 'BAD_JSON';

    final authError = _authenticate(
      sig,
      (nonce) => '$nonce|CONFIG|${min.toStringAsFixed(1)}|${max.toStringAsFixed(1)}'
          '|$auto|$maxDur|${minWater.toStringAsFixed(1)}',
    );
    if (authError != null) return authError;

    final isValid = min >= 0 && min < max && max <= 100 && (auto == 0 || auto == 1) &&
        maxDur >= 60 && maxDur <= 600 && minWater >= 20 && minWater <= 100;
    if (!isValid) return 'OUT_OF_RANGE';
    if (isPumpOn) return 'PUMP_BUSY';

    soilMin = min;
    soilMax = max;
    isAutoMode = auto == 1;
    maxRunSeconds = maxDur;
    minWaterPercent = minWater;
    return 'OK';
  }

  String _handleTime(List<int> value) {
    final json = _decode(value);
    final unix = json?['unix'];
    final sig = json?['sig'];
    if (unix is! int || sig is! String) return 'BAD_JSON';

    final authError = _authenticate(sig, (nonce) => '$nonce|TIME|$unix');
    if (authError != null) return authError;

    if (unix < _unixMin || unix > _unixMax) return 'INVALID_TIME';
    syncedUnixSeconds = unix;
    return 'OK';
  }

  String _handleWifi(List<int> value) {
    if (!isBonded) {
      _consumeNonce();
      throw const BleWriteRejectedException('Chưa ghép nối với ESP32');
    }
    final json = _decode(value);
    final ssid = json?['ssid'];
    final pass = json?['pass'];
    final sig = json?['sig'];
    if (ssid is! String || pass is! String || sig is! String) return 'BAD_JSON';

    final authError = _authenticate(sig, (nonce) => '$nonce|WIFI|$ssid|$pass');
    if (authError != null) return authError;

    final ssidBytes = utf8.encode(ssid).length;
    final passBytes = utf8.encode(pass).length;
    final isValid = ssidBytes >= 1 && ssidBytes <= 32 && passBytes >= 8 && passBytes <= 63 &&
        !ssid.contains('|') && !pass.contains('|');
    if (!isValid) return 'OUT_OF_RANGE';
    if (!wifiConnects) return 'WIFI_FAIL';
    savedSsid = ssid;
    return 'WIFI_OK';
  }

  void _consumeNonce() {
    _nonce = null;
    _nonceIssuedAt = null;
  }

  /// Nonce chỉ dùng một lần, bị tiêu thụ kể cả khi chữ ký sai.
  String? _authenticate(String sig, String Function(String nonce) buildMessage) {
    final nonce = _nonce;
    final issuedAt = _nonceIssuedAt;
    _consumeNonce();
    if (nonce == null || issuedAt == null) return 'NONCE_EXPIRED';
    if (_now().difference(issuedAt) >= nonceLifetime) return 'NONCE_EXPIRED';

    final digest = Hmac(sha256, utf8.encode(secretKey)).convert(utf8.encode(buildMessage(nonce)));
    return digest.toString().substring(0, 16) == sig ? null : 'BAD_SIG';
  }

  Map<String, dynamic>? _decode(List<int> value) {
    try {
      final decoded = jsonDecode(utf8.decode(value));
      return decoded is Map<String, dynamic> ? decoded : null;
    } on FormatException {
      return null;
    }
  }
}
