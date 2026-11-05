import 'dart:async';
import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/ble/domain/ble_command_service.dart';
import 'package:watering_app/features/ble/domain/ble_commands.dart';
import 'package:watering_app/features/ble/domain/ble_messages.dart';
import 'package:watering_app/features/ble/domain/ble_protocol.dart';

import '../../../../tool/fake_esp32.dart';

const _key = 'test-ble-secret-key';

/// Trả về nonce cố định hoặc giữ lại phản hồi để kiểm tra các tình huống khó tạo bằng ESP32 giả.
class _ScriptedEsp32 extends FakeEsp32 {
  _ScriptedEsp32() : super(secretKey: _key);

  String? nonceOverride;
  void Function()? onNonceRead;

  @override
  Future<List<int>> read(String characteristicUuid) async {
    if (characteristicUuid == BleUuids.nonce) {
      onNonceRead?.call();
      if (nonceOverride != null) return utf8.encode(nonceOverride!);
    }
    return super.read(characteristicUuid);
  }
}

void main() {
  late FakeEsp32 esp32;
  late BleCommandService service;

  setUp(() {
    esp32 = FakeEsp32(secretKey: _key);
    service = BleCommandService(
      transport: esp32,
      secretKey: _key,
      resultTimeout: const Duration(milliseconds: 50),
    );
  });

  group('Lệnh thành công', () {
    test('bật rồi tắt bơm', () async {
      final on = await service.send(BlePumpOnCommand(60));
      expect(on, const BleCommandResult(op: 'PUMP_ON', code: BleResultCode.ok));
      expect(esp32.isPumpOn, isTrue);

      final off = await service.send(const BlePumpOffCommand());
      expect(off.isSuccess, isTrue);
      expect(esp32.isPumpOn, isFalse);
    });

    test('cấu hình được ESP32 lưu', () async {
      final result = await service.send(BleConfigCommand(
        soilMin: 30.5,
        soilMax: 70,
        autoMode: false,
        maxDurationSeconds: 300,
        minWaterCm: 7.5,
      ));
      expect(result.isSuccess, isTrue);
      expect(esp32.soilMin, 30.5);
      expect(esp32.autoMode, isFalse);
      expect(esp32.maxDurationSeconds, 300);
      expect(esp32.minWaterCm, 7.5);
    });

    test('đồng bộ giờ', () async {
      final result = await service.send(BleTimeSyncCommand(1793800000));
      expect(result.isSuccess, isTrue);
      expect(esp32.syncedUnixSeconds, 1793800000);
    });

    test('mỗi lệnh dùng một nonce mới', () async {
      await service.send(BlePumpOnCommand(60));
      final second = await service.send(const BlePumpOffCommand());
      expect(second.code, BleResultCode.ok);
    });
  });

  group('ESP32 từ chối', () {
    test('sai khóa trả BAD_SIG và không bật bơm', () async {
      final wrongKey = BleCommandService(transport: esp32, secretKey: 'khoa-sai');
      final result = await wrongKey.send(BlePumpOnCommand(60));
      expect(result.code, BleResultCode.badSig);
      expect(esp32.isPumpOn, isFalse);
    });

    test('nonce quá 30 giây trả NONCE_EXPIRED', () async {
      var now = DateTime(2026, 11, 6);
      final late = _LateWriteEsp32(now: () => now, onWrite: () {
        now = now.add(const Duration(seconds: 31));
      });
      final lateService = BleCommandService(transport: late, secretKey: _key);
      final result = await lateService.send(BlePumpOnCommand(60));
      expect(result.code, BleResultCode.nonceExpired);
    });

    test('vượt max_dur trả OUT_OF_RANGE', () async {
      final result = await service.send(BlePumpOnCommand(121));
      expect(result.code, BleResultCode.outOfRange);
    });

    test('nước thấp trả LOW_WATER', () async {
      esp32.waterLevelCm = 1;
      final result = await service.send(BlePumpOnCommand(60));
      expect(result.code, BleResultCode.lowWater);
    });

    test('cảm biến nước lỗi trả SENSOR_ERROR', () async {
      esp32.waterLevelCm = null;
      final result = await service.send(BlePumpOnCommand(60));
      expect(result.code, BleResultCode.sensorError);
    });

    test('bơm đang chạy: bật trả PUMP_BUSY, tắt vẫn được', () async {
      await service.send(BlePumpOnCommand(60));
      final busy = await service.send(BlePumpOnCommand(60));
      expect(busy.code, BleResultCode.pumpBusy);

      final off = await service.send(const BlePumpOffCommand());
      expect(off.code, BleResultCode.ok);
    });
  });

  group('Không có kết quả', () {
    test('hết giờ thì báo lỗi và không tự gửi lại', () async {
      esp32.dropResults = true;
      await expectLater(
        service.send(BlePumpOnCommand(60)),
        throwsA(isA<BleCommandTimeoutException>()),
      );
      expect(esp32.writeCount, 1);
    });

    test('sau khi hết giờ vẫn gửi được lệnh khác', () async {
      esp32.dropResults = true;
      await expectLater(service.send(BlePumpOnCommand(60)), throwsA(anything));
      esp32.dropResults = false;

      final off = await service.send(const BlePumpOffCommand());
      expect(off.isSuccess, isTrue);
    });

    test('mất kết nối khi đang chờ', () async {
      esp32.dropResults = true;
      final pending = service.send(BlePumpOnCommand(60));
      final expectation = expectLater(pending, throwsA(isA<BleDisconnectedException>()));
      await Future<void>.delayed(Duration.zero);
      await esp32.disconnect();
      await expectation;
    });
  });

  group('Quy tắc gửi lệnh', () {
    test('chỉ một lệnh tại một thời điểm', () async {
      esp32.dropResults = true;
      final first = service.send(BlePumpOnCommand(60));
      await expectLater(
        service.send(const BlePumpOffCommand()),
        throwsA(isA<BleCommandInProgressException>()),
      );
      await expectLater(first, throwsA(isA<BleCommandTimeoutException>()));
      expect(esp32.writeCount, 1);
    });

    test('bỏ qua kết quả của lệnh khác và gói hỏng', () async {
      esp32.dropResults = true;
      final pending = service.send(BlePumpOnCommand(60));
      await Future<void>.delayed(Duration.zero);
      esp32.emitResult('PUMP_OFF', 'OK');
      esp32.emitRawResult(utf8.encode('khong-phai-json'));
      esp32.emitResult('PUMP_ON', 'LOW_WATER');
      expect((await pending).code, BleResultCode.lowWater);
    });

    test('bỏ qua kết quả muộn đến trước khi ghi lệnh mới', () async {
      final scripted = _ScriptedEsp32()..dropResults = true;
      final scriptedService = BleCommandService(
        transport: scripted,
        secretKey: _key,
        resultTimeout: const Duration(milliseconds: 50),
      );
      scripted.onNonceRead = () => scripted.emitResult('PUMP_ON', 'OK');
      await expectLater(
        scriptedService.send(BlePumpOnCommand(60)),
        throwsA(isA<BleCommandTimeoutException>()),
      );
    });

    test('nonce sai định dạng thì không ghi lệnh', () async {
      final scripted = _ScriptedEsp32()..nonceOverride = 'ABC';
      final scriptedService = BleCommandService(transport: scripted, secretKey: _key);
      await expectLater(
        scriptedService.send(BlePumpOnCommand(60)),
        throwsA(isA<BleInvalidNonceException>()),
      );
      expect(scripted.writeCount, 0);
    });

    test('lệnh vượt MTU thì không ghi', () async {
      final small = FakeEsp32(secretKey: _key, maxWritePayloadBytes: 20);
      final smallService = BleCommandService(transport: small, secretKey: _key);
      await expectLater(
        smallService.send(BlePumpOnCommand(60)),
        throwsA(isA<BlePayloadTooLargeException>()),
      );
      expect(small.writeCount, 0);
    });
  });

  group('Cảm biến', () {
    test('đọc một lần', () async {
      final sample = await service.readSensors();
      expect(sample.waterLevelCm, 25.4);
      expect(sample.isPumpOn, isFalse);
    });

    test('nhận liên tục và bỏ qua gói hỏng', () async {
      final samples = <BleSensorSample>[];
      final done = Completer<void>();
      final subscription = service.watchSensors().listen(samples.add, onDone: done.complete);
      await Future<void>.delayed(Duration.zero);

      esp32.emitSensorPacket();
      esp32.emitRawSensorPacket(utf8.encode('{"soil":'));
      esp32.isPumpOn = true;
      esp32.emitSensorPacket();
      await esp32.disconnect();
      await done.future;
      await subscription.cancel();

      expect(samples.map((s) => s.isPumpOn), [false, true]);
    });
  });
}

/// Thời gian trôi qua giữa lúc đọc nonce và lúc ghi lệnh.
class _LateWriteEsp32 extends FakeEsp32 {
  _LateWriteEsp32({required DateTime Function() now, required this.onWrite})
      : super(secretKey: _key, now: now);

  final void Function() onWrite;

  @override
  Future<void> write(String characteristicUuid, List<int> value) {
    onWrite();
    return super.write(characteristicUuid, value);
  }
}
