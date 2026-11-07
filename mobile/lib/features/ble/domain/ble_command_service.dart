import 'dart:async';
import 'dart:convert';

import 'ble_commands.dart';
import 'ble_messages.dart';
import 'ble_protocol.dart';
import 'ble_transport.dart';

/// Lỗi khi gửi lệnh mà chưa có kết quả nghiệp vụ từ ESP32.
sealed class BleCommandException implements Exception {
  const BleCommandException(this.message);

  final String message;

  @override
  String toString() => message;
}

class BleCommandInProgressException extends BleCommandException {
  const BleCommandInProgressException() : super('Đang có lệnh BLE chờ kết quả.');
}

/// Không rõ lệnh đã chạy hay chưa, nên phải kiểm tra trạng thái trước khi gửi lại.
class BleCommandTimeoutException extends BleCommandException {
  const BleCommandTimeoutException() : super('ESP32 không trả kết quả đúng hạn.');
}

class BleDisconnectedException extends BleCommandException {
  const BleDisconnectedException() : super('Mất kết nối BLE khi đang chờ kết quả.');
}

class BleInvalidNonceException extends BleCommandException {
  const BleInvalidNonceException() : super('Nonce từ ESP32 không đúng định dạng.');
}

class BlePayloadTooLargeException extends BleCommandException {
  const BlePayloadTooLargeException() : super('Lệnh vượt quá kích thước MTU hiện tại.');
}

/// ESP32 trả lỗi ATT thay vì kết quả qua `0005`. Firmware làm vậy khi không có mã kết quả phù hợp:
/// đang tự động, đang chờ giữa hai lần tưới, bị khóa, chưa ghép nối, chưa bật Notify
/// hoặc MTU quá nhỏ. Lớp kết nối BLE thật phải ném lỗi này khi ghi bị từ chối.
class BleWriteRejectedException extends BleCommandException {
  const BleWriteRejectedException([
    super.message = 'Thiết bị không thực hiện lệnh. Kiểm tra chế độ tự động, ghép nối hoặc thử lại sau.',
  ]);
}

/// Gửi lệnh theo quy trình đã chốt: bật Notify `0005`, đọc nonce mới, ký, ghi, chờ kết quả.
/// Mỗi lúc chỉ một lệnh và không bao giờ tự gửi lại.
class BleCommandService {
  BleCommandService({
    required BleTransport transport,
    required String secretKey,
    Duration resultTimeout = defaultResultTimeout,
  })  : _transport = transport,
        _secretKey = secretKey,
        _resultTimeout = resultTimeout;

  static const Duration defaultResultTimeout = Duration(seconds: 3);
  static final RegExp _noncePattern = RegExp(r'^[0-9a-f]{16}$');

  final BleTransport _transport;
  final String _secretKey;
  final Duration _resultTimeout;
  bool _isBusy = false;

  Future<BleCommandResult> send(BleCommand command) async {
    if (_isBusy) throw const BleCommandInProgressException();
    _isBusy = true;
    try {
      return await _execute(command);
    } finally {
      _isBusy = false;
    }
  }

  /// Đọc một gói cảm biến. Ném [FormatException] nếu dữ liệu sai định dạng.
  Future<BleSensorSample> readSensors() async =>
      BleSensorSample.fromBytes(await _transport.read(BleUuids.sensor));

  /// Luồng cảm biến từ Notify `0001`; gói sai định dạng bị bỏ qua.
  Stream<BleSensorSample> watchSensors() async* {
    final packets = await _transport.subscribe(BleUuids.sensor);
    await for (final packet in packets) {
      final sample = _tryParse(() => BleSensorSample.fromBytes(packet));
      if (sample != null) yield sample;
    }
  }

  Future<BleCommandResult> _execute(BleCommand command) async {
    final completer = Completer<BleCommandResult>();
    // Bỏ qua kết quả đến trước lúc ghi: đó là kết quả muộn của lệnh trước.
    var isAccepting = false;

    final results = await _transport.subscribe(BleUuids.result);
    final subscription = results.listen(
      (packet) {
        final result = _tryParse(() => BleCommandResult.fromBytes(packet));
        if (isAccepting && result?.op == command.op && !completer.isCompleted) {
          completer.complete(result);
        }
      },
      onError: (Object error, StackTrace stack) {
        if (!completer.isCompleted) completer.completeError(error, stack);
      },
      onDone: () {
        if (!completer.isCompleted) completer.completeError(const BleDisconnectedException());
      },
    );

    try {
      final payload = command.encode(nonce: await _readNonce(), secretKey: _secretKey);
      if (payload.length > _transport.maxWritePayloadBytes) {
        throw const BlePayloadTooLargeException();
      }
      isAccepting = true;
      await _transport.write(command.characteristicUuid, payload);
      return await completer.future.timeout(command.resultTimeout ?? _resultTimeout);
    } on TimeoutException {
      throw const BleCommandTimeoutException();
    } finally {
      await subscription.cancel();
    }
  }

  Future<String> _readNonce() async {
    final nonce = utf8.decode(await _transport.read(BleUuids.nonce));
    if (!_noncePattern.hasMatch(nonce)) throw const BleInvalidNonceException();
    return nonce;
  }

  T? _tryParse<T>(T Function() parse) {
    try {
      return parse();
    } on FormatException {
      return null;
    }
  }
}
