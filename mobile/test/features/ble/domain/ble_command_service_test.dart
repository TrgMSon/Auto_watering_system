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
      final on = await service.send(BlePumpOnCommand(5));
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
        autoMode: true,
        maxDurationSeconds: 300,
        minWaterPercent: 25,
      ));
      expect(result, const BleCommandResult(op: 'CONFIG', code: BleResultCode.ok));
      expect(esp32.soilMin, 30.5);
      expect(esp32.isAutoMode, isTrue);
      expect(esp32.maxRunSeconds, 300);
      expect(esp32.minWaterPercent, 25);
    });

    test('cấu hình khi bơm đang chạy trả PUMP_BUSY', () async {
      esp32.isPumpOn = true;
      final result = await service.send(BleConfigCommand(
        soilMin: 30,
        soilMax: 70,
        autoMode: false,
        maxDurationSeconds: 120,
        minWaterPercent: 20,
      ));
      expect(result.code, BleResultCode.pumpBusy);
    });

    test('đồng bộ giờ', () async {
      final result = await service.send(BleTimeSyncCommand(1793800000));
      expect(result.isSuccess, isTrue);
      expect(esp32.syncedUnixSeconds, 1793800000);
    });

    test('mỗi lệnh dùng một nonce mới', () async {
      await service.send(BlePumpOnCommand(5));
      final second = await service.send(const BlePumpOffCommand());
      expect(second.code, BleResultCode.ok);
    });
  });

  group('ESP32 từ chối', () {
    test('sai khóa trả BAD_SIG và không bật bơm', () async {
      final wrongKey = BleCommandService(transport: esp32, secretKey: 'khoa-sai');
      final result = await wrongKey.send(BlePumpOnCommand(5));
      expect(result.code, BleResultCode.badSig);
      expect(esp32.isPumpOn, isFalse);
    });

    test('nonce quá 30 giây trả NONCE_EXPIRED', () async {
      var now = DateTime(2026, 11, 6);
      final late = _LateWriteEsp32(now: () => now, onWrite: () {
        now = now.add(const Duration(seconds: 31));
      });
      final lateService = BleCommandService(transport: late, secretKey: _key);
      final result = await lateService.send(BlePumpOnCommand(5));
      expect(result.code, BleResultCode.nonceExpired);
    });

    test('quá 5 giây trả OUT_OF_RANGE', () async {
      final result = await service.send(BlePumpOnCommand(6));
      expect(result.code, BleResultCode.outOfRange);
    });

    test('nước thấp trả LOW_WATER', () async {
      esp32.waterPercent = 10;
      final result = await service.send(BlePumpOnCommand(5));
      expect(result.code, BleResultCode.lowWater);
    });

    test('cảm biến lỗi trả SENSOR_ERROR', () async {
      esp32.isSoilValid = false;
      final result = await service.send(BlePumpOnCommand(5));
      expect(result.code, BleResultCode.sensorError);
    });

    test('bơm đang chạy: bật trả PUMP_BUSY, tắt vẫn được', () async {
      await service.send(BlePumpOnCommand(5));
      final busy = await service.send(BlePumpOnCommand(5));
      expect(busy.code, BleResultCode.pumpBusy);

      final off = await service.send(const BlePumpOffCommand());
      expect(off.code, BleResultCode.ok);
    });
  });

  group('Firmware không có mã kết quả', () {
    test('bật bơm khi đang tự động thì ghi bị từ chối', () async {
      esp32.isAutoMode = true;
      await expectLater(
        service.send(BlePumpOnCommand(5)),
        throwsA(isA<BleWriteRejectedException>()),
      );
      expect(esp32.isPumpOn, isFalse);
    });

    test('đang chờ giữa hai lần tưới hoặc bị khóa thì ghi bị từ chối', () async {
      esp32.isBlockedByController = true;
      await expectLater(
        service.send(BlePumpOnCommand(5)),
        throwsA(isA<BleWriteRejectedException>()),
      );
    });

    test('tắt bơm khi đang tự động vẫn được và chuyển về thủ công', () async {
      esp32.isAutoMode = true;
      final result = await service.send(const BlePumpOffCommand());
      expect(result.isSuccess, isTrue);
      expect(esp32.isAutoMode, isFalse);
    });

    test('sau khi bị từ chối vẫn gửi được lệnh khác', () async {
      esp32.isAutoMode = true;
      await expectLater(service.send(BlePumpOnCommand(5)), throwsA(anything));
      final off = await service.send(const BlePumpOffCommand());
      expect(off.isSuccess, isTrue);
    });
  });

  group('Wi-Fi qua 0006', () {
    test('kết nối được thì trả WIFI_OK', () async {
      final result = await service.send(BleWifiCommand(ssid: 'VuonNha', password: 'matkhau123'));
      expect(result, const BleCommandResult(op: 'WIFI', code: BleResultCode.wifiOk));
      expect(esp32.savedSsid, 'VuonNha');
    });

    test('không kết nối được thì trả WIFI_FAIL', () async {
      esp32.wifiConnects = false;
      final result = await service.send(BleWifiCommand(ssid: 'VuonNha', password: 'matkhau123'));
      expect(result.code, BleResultCode.wifiFail);
      expect(esp32.savedSsid, isNull);
    });

    test('chưa ghép nối thì ghi bị từ chối', () async {
      esp32.isBonded = false;
      await expectLater(
        service.send(BleWifiCommand(ssid: 'VuonNha', password: 'matkhau123')),
        throwsA(isA<BleWriteRejectedException>()),
      );
    });

    test('tên có dấu ngoặc kép vẫn ký đúng sau khi mã hóa JSON', () async {
      final result = await service.send(BleWifiCommand(ssid: 'Nhà "Lan"', password: r'mat\khau123'));
      expect(result.code, BleResultCode.wifiOk);
      expect(esp32.savedSsid, 'Nhà "Lan"');
    });
  });

  test('giờ ngoài khoảng 2024–2099 trả INVALID_TIME', () async {
    final result = await service.send(BleTimeSyncCommand(1600000000));
    expect(result.code, BleResultCode.invalidTime);
  });

  group('Không có kết quả', () {
    test('hết giờ thì báo lỗi và không tự gửi lại', () async {
      esp32.dropResults = true;
      await expectLater(
        service.send(BlePumpOnCommand(5)),
        throwsA(isA<BleCommandTimeoutException>()),
      );
      expect(esp32.writeCount, 1);
    });

    test('sau khi hết giờ vẫn gửi được lệnh khác', () async {
      esp32.dropResults = true;
      await expectLater(service.send(BlePumpOnCommand(5)), throwsA(anything));
      esp32.dropResults = false;

      final off = await service.send(const BlePumpOffCommand());
      expect(off.isSuccess, isTrue);
    });

    test('mất kết nối khi đang chờ', () async {
      esp32.dropResults = true;
      final pending = service.send(BlePumpOnCommand(5));
      final expectation = expectLater(pending, throwsA(isA<BleDisconnectedException>()));
      await Future<void>.delayed(Duration.zero);
      await esp32.disconnect();
      await expectation;
    });
  });

  group('Quy tắc gửi lệnh', () {
    test('chỉ một lệnh tại một thời điểm', () async {
      esp32.dropResults = true;
      final first = service.send(BlePumpOnCommand(5));
      await expectLater(
        service.send(const BlePumpOffCommand()),
        throwsA(isA<BleCommandInProgressException>()),
      );
      await expectLater(first, throwsA(isA<BleCommandTimeoutException>()));
      expect(esp32.writeCount, 1);
    });

    test('bỏ qua kết quả của lệnh khác và gói hỏng', () async {
      esp32.dropResults = true;
      final pending = service.send(BlePumpOnCommand(5));
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
        scriptedService.send(BlePumpOnCommand(5)),
        throwsA(isA<BleCommandTimeoutException>()),
      );
    });

    test('nonce sai định dạng thì không ghi lệnh', () async {
      final scripted = _ScriptedEsp32()..nonceOverride = 'ABC';
      final scriptedService = BleCommandService(transport: scripted, secretKey: _key);
      await expectLater(
        scriptedService.send(BlePumpOnCommand(5)),
        throwsA(isA<BleInvalidNonceException>()),
      );
      expect(scripted.writeCount, 0);
    });

    test('lệnh vượt MTU thì không ghi', () async {
      final small = FakeEsp32(secretKey: _key, maxWritePayloadBytes: 20);
      final smallService = BleCommandService(transport: small, secretKey: _key);
      await expectLater(
        smallService.send(BlePumpOnCommand(5)),
        throwsA(isA<BlePayloadTooLargeException>()),
      );
      expect(small.writeCount, 0);
    });
  });

  group('Cảm biến', () {
    test('đọc một lần', () async {
      final sample = await service.readSensors();
      expect(sample.waterLevelPercent, 60.0);
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
