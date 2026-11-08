import 'dart:convert';

import 'package:flutter_test/flutter_test.dart';
import 'package:watering_app/features/automation/data/models/device_config_model.dart';
import 'package:watering_app/features/ble/domain/ble_commands.dart';
import 'package:watering_app/features/ble/domain/ble_messages.dart';
import 'package:watering_app/features/ble/domain/ble_protocol.dart';
import 'package:watering_app/features/ble/domain/ble_signer.dart';

const _key = 'test-ble-secret-key';
const _nonce = '84a9f21b3c0e5a1d';

String _signed(BleCommand command) => BleSigner.sign(_key, command.signingMessage(_nonce));

void main() {
  group('Dữ liệu mẫu trong hợp đồng BLE', () {
    test('lệnh bơm', () {
      expect(_signed(BlePumpOnCommand(300)), '48ba618920e3dcb4');
      expect(_signed(const BlePumpOffCommand()), '238041d3721ef169');
    });

    test('cấu hình đủ 5 trường', () {
      final command = BleConfigCommand(
        soilMin: 30,
        soilMax: 70,
        autoMode: true,
        maxDurationSeconds: 600,
        minWaterPercent: 20,
      );
      expect(command.signingMessage(_nonce), '$_nonce|CONFIG|30.0|70.0|1|600|20.0');
      expect(_signed(command), '5313188c56af17c5');
    });

    test('đồng bộ giờ', () {
      expect(_signed(BleTimeSyncCommand(1793800000)), '26b87998f4bc4778');
    });

    test('cấu hình 3 trường cũ vẫn khớp thuật toán đã chạy trên ESP32', () {
      expect(BleSigner.sign(_key, '$_nonce|CONFIG|30.0|70.0|1'), '9c77373f7323c7ac');
    });
  });

  group('Định dạng gửi đi', () {
    Map<String, dynamic> decode(BleCommand command) =>
        jsonDecode(utf8.decode(command.encode(nonce: _nonce, secretKey: _key)))
            as Map<String, dynamic>;

    test('lệnh bơm', () {
      expect(utf8.decode(BlePumpOnCommand(300).encode(nonce: _nonce, secretKey: _key)),
          '{"cmd":"PUMP_ON","dur":300,"sig":"48ba618920e3dcb4"}');
      expect(utf8.decode(const BlePumpOffCommand().encode(nonce: _nonce, secretKey: _key)),
          '{"cmd":"PUMP_OFF","dur":0,"sig":"238041d3721ef169"}');
    });

    test('cấu hình giữ một chữ số thập phân', () {
      final command = BleConfigCommand(
        soilMin: 30,
        soilMax: 70,
        autoMode: true,
        maxDurationSeconds: 600,
        minWaterPercent: 20,
      );
      expect(utf8.decode(command.encode(nonce: _nonce, secretKey: _key)),
          '{"min":30.0,"max":70.0,"auto":1,"max_dur":600,"min_water":20.0,"sig":"5313188c56af17c5"}');
    });

    test('chữ ký và JSON cùng dùng giá trị đã làm tròn', () {
      final command = BleConfigCommand(
        soilMin: 30.26,
        soilMax: 70,
        autoMode: false,
        maxDurationSeconds: 120,
        minWaterPercent: 20,
      );
      expect(command.signingMessage(_nonce), '$_nonce|CONFIG|30.3|70.0|0|120|20.0');
      expect(decode(command)['min'], 30.3);
    });

    test('đổi cấu hình từ phút sang giây', () {
      const config = DeviceConfigModel(
        deviceId: '38:3E:51:6F:C7:A8',
        autoMode: true,
        soilMoistureMin: 40,
        soilMoistureMax: 80,
        airHumidityMin: 50,
        airHumidityMax: 80,
        temperatureMin: 20,
        temperatureMax: 35,
        maxWateringDuration: 2,
        waterLevelMin: 25,
      );
      final command = BleConfigCommand.fromDeviceConfig(config);
      expect(command.maxDurationSeconds, 120);
      expect(command.minWaterPercent, 25.0);
    });
  });

  group('Từ chối giá trị ngoài hợp đồng', () {
    BleConfigCommand config({double min = 30, double max = 70, int dur = 120, double water = 20}) =>
        BleConfigCommand(
          soilMin: min,
          soilMax: max,
          autoMode: true,
          maxDurationSeconds: dur,
          minWaterPercent: water,
        );

    test('thời gian bơm', () {
      expect(() => BlePumpOnCommand(0), throwsArgumentError);
      expect(() => BlePumpOnCommand(601), throwsArgumentError);
    });

    test('ngưỡng độ ẩm', () {
      expect(() => config(min: 70, max: 70), throwsArgumentError);
      expect(() => config(min: -1), throwsArgumentError);
      expect(() => config(max: 101), throwsArgumentError);
      // Hai giá trị khác nhau nhưng bằng nhau sau khi làm tròn.
      expect(() => config(min: 69.96, max: 70), throwsArgumentError);
    });

    test('max_dur và mực nước', () {
      expect(() => config(dur: 59), throwsArgumentError);
      expect(() => config(dur: 601), throwsArgumentError);
      expect(() => config(water: 19), throwsArgumentError);
      expect(() => config(water: 101), throwsArgumentError);
      expect(config(dur: 60).maxDurationSeconds, 60);
      expect(config(dur: 600).maxDurationSeconds, 600);
    });

    test('giờ không hợp lệ', () {
      expect(() => BleTimeSyncCommand(0), throwsArgumentError);
    });
  });

  test('giờ luôn đổi sang UTC', () {
    final local = DateTime.utc(2026, 11, 6, 3).toLocal();
    expect(BleTimeSyncCommand.fromDateTime(local).unixSeconds,
        DateTime.utc(2026, 11, 6, 3).millisecondsSinceEpoch ~/ 1000);
  });

  group('Đọc dữ liệu từ ESP32', () {
    test('gói cảm biến', () {
      final sample = BleSensorSample.fromBytes(
          utf8.encode('{"soil":42.5,"temp":31.8,"hum":68.0,"water":25.4,"pump":1}'));
      expect(sample.soilMoisture, 42.5);
      expect(sample.waterLevelPercent, 25.4);
      expect(sample.isPumpOn, isTrue);
    });

    test('cảm biến lỗi là null, không phải 0', () {
      final sample = BleSensorSample.fromBytes(
          utf8.encode('{"soil":null,"temp":31,"hum":null,"water":25.4,"pump":0}'));
      expect(sample.soilMoisture, isNull);
      expect(sample.temperature, 31.0);
      expect(sample.isPumpOn, isFalse);
    });

    test('gói sai định dạng', () {
      expect(() => BleSensorSample.fromBytes(utf8.encode('{"soil":"x","pump":0}')),
          throwsFormatException);
      expect(() => BleSensorSample.fromBytes(utf8.encode('{"soil":1}')), throwsFormatException);
      expect(() => BleSensorSample.fromBytes(utf8.encode('[1]')), throwsFormatException);
      expect(() => BleCommandResult.fromBytes(utf8.encode('{"op":"X"}')), throwsFormatException);
    });

    test('đủ 13 mã kết quả, mã lạ không làm hỏng luồng', () {
      const codes = [
        'OK', 'BAD_SIG', 'NONCE_EXPIRED', 'BAD_JSON', 'OUT_OF_RANGE', 'LOW_WATER',
        'MAINTENANCE', 'SENSOR_ERROR', 'PUMP_BUSY', 'INVALID_TIME', 'WIFI_OK', 'WIFI_FAIL', 'STORAGE_ERROR',
      ];
      for (final code in codes) {
        expect(BleResultCode.fromWire(code).wire, code);
      }
      expect(BleResultCode.fromWire('BUSY'), BleResultCode.unknown);
    });
  });

  group('Wi-Fi', () {
    test('chuỗi ký và JSON theo firmware', () {
      final command = BleWifiCommand(ssid: 'VuonNha', password: 'matkhau123');
      expect(command.signingMessage(_nonce), '$_nonce|WIFI|VuonNha|matkhau123');
      expect(_signed(command), 'a12ba07aa2ff323f');
      expect(utf8.decode(command.encode(nonce: _nonce, secretKey: _key)),
          '{"ssid":"VuonNha","pass":"matkhau123","sig":"a12ba07aa2ff323f"}');
      expect(command.resultTimeout, const Duration(seconds: 35));
    });

    test('từ chối giá trị firmware không nhận', () {
      expect(() => BleWifiCommand(ssid: '', password: 'matkhau123'), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'a' * 33, password: 'matkhau123'), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'VuonNha', password: 'ngan'), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'VuonNha', password: ''), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'VuonNha', password: 'a' * 64), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'Vuon|Nha', password: 'matkhau123'), throwsArgumentError);
      expect(() => BleWifiCommand(ssid: 'VuonNha', password: 'mat|khau123'), throwsArgumentError);
    });

    test('độ dài tính theo byte UTF-8', () {
      // 11 chữ "ệ" là 33 byte.
      expect(() => BleWifiCommand(ssid: 'ệ' * 11, password: 'matkhau123'), throwsArgumentError);
      expect(BleWifiCommand(ssid: 'ệ' * 10, password: 'matkhau123').ssid.length, 10);
    });
  });

  group('Mã thiết bị', () {
    test('đổi từ tên BLE sang MAC và ngược lại', () {
      expect(BleDeviceId.fromAdvertisingName('WS-383E516FC7A8'), '38:3E:51:6F:C7:A8');
      expect(BleDeviceId.toAdvertisingName('38:3E:51:6F:C7:A8'), 'WS-383E516FC7A8');
    });

    test('bỏ qua tên không thuộc hệ thống', () {
      expect(BleDeviceId.fromAdvertisingName('Tai nghe'), isNull);
      expect(BleDeviceId.fromAdvertisingName('WS-383E516FC7A'), isNull);
      expect(BleDeviceId.fromAdvertisingName('WS-383e516fc7a8'), isNull);
    });
  });
}
