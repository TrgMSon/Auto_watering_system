import 'dart:async';
import 'dart:convert';
import 'dart:math';

import 'package:crypto/crypto.dart';
import 'package:watering_app/features/ble/domain/ble_protocol.dart';
import 'package:watering_app/features/ble/domain/ble_transport.dart';

/// ESP32 giả để chạy thử luồng BLE khi firmware thật chưa có.
/// Cố ý tự kiểm tra chữ ký bằng code riêng, để lỗi ở phía app không bị che khuất.
class FakeEsp32 implements BleTransport {
  FakeEsp32({
    required this.secretKey,
    DateTime Function()? now,
    this.maxWritePayloadBytes = 182,
  }) : _now = now ?? DateTime.now;

  static const Duration nonceLifetime = Duration(seconds: 30);

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
  int maxDurationSeconds = 120;
  double minWaterCm = 5.0;
  double? waterLevelCm = 25.4;
  double? soilMin;
  double? soilMax;
  bool? autoMode;
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
    final (op, code) = switch (characteristicUuid) {
      BleUuids.pump => _handlePump(value),
      BleUuids.config => ('CONFIG', _handleConfig(value)),
      BleUuids.time => ('TIME', _handleTime(value)),
      _ => throw UnsupportedError('Không ghi được $characteristicUuid'),
    };
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
        'soil': 42.5,
        'temp': 31.8,
        'hum': 68.0,
        'water': waterLevelCm,
        'pump': isPumpOn ? 1 : 0,
      }));

  (String, String) _handlePump(List<int> value) {
    final json = _decode(value);
    final cmd = json?['cmd'];
    final dur = json?['dur'];
    final sig = json?['sig'];
    if (cmd is! String || dur is! int || sig is! String) return ('PUMP', 'BAD_JSON');

    final authError = _authenticate(sig, (nonce) => '$nonce|$cmd|$dur');
    if (authError != null) return (cmd, authError);

    switch (cmd) {
      case 'PUMP_OFF':
        if (dur != 0) return (cmd, 'OUT_OF_RANGE');
        isPumpOn = false;
      case 'PUMP_ON':
        if (dur < 1 || dur > maxDurationSeconds) return (cmd, 'OUT_OF_RANGE');
        final water = waterLevelCm;
        if (water == null) return (cmd, 'SENSOR_ERROR');
        if (water < minWaterCm) return (cmd, 'LOW_WATER');
        if (isPumpOn) return (cmd, 'PUMP_BUSY');
        isPumpOn = true;
      default:
        return (cmd, 'BAD_JSON');
    }
    return (cmd, 'OK');
  }

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

    final isValid = min >= 0 && min < max && max <= 100 && (auto == 0 || auto == 1);
    if (!isValid || maxDur < 60 || maxDur > 600) return 'OUT_OF_RANGE';

    soilMin = min;
    soilMax = max;
    autoMode = auto == 1;
    maxDurationSeconds = maxDur;
    minWaterCm = minWater;
    return 'OK';
  }

  String _handleTime(List<int> value) {
    final json = _decode(value);
    final unix = json?['unix'];
    final sig = json?['sig'];
    if (unix is! int || sig is! String) return 'BAD_JSON';

    final authError = _authenticate(sig, (nonce) => '$nonce|TIME|$unix');
    if (authError != null) return authError;

    if (unix <= 0) return 'INVALID_TIME';
    syncedUnixSeconds = unix;
    return 'OK';
  }

  /// Nonce chỉ dùng một lần, bị tiêu thụ kể cả khi chữ ký sai.
  String? _authenticate(String sig, String Function(String nonce) buildMessage) {
    final nonce = _nonce;
    final issuedAt = _nonceIssuedAt;
    _nonce = null;
    _nonceIssuedAt = null;
    if (nonce == null || issuedAt == null) return 'NONCE_EXPIRED';
    if (_now().difference(issuedAt) > nonceLifetime) return 'NONCE_EXPIRED';

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
