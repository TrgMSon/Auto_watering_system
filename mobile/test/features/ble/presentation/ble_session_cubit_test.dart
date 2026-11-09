import 'dart:async';

import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/ble/data/ble_key_repository.dart';
import 'package:watering_app/features/ble/domain/ble_connector.dart';
import 'package:watering_app/features/ble/domain/ble_transport.dart';
import 'package:watering_app/features/ble/presentation/bloc/ble_session_cubit.dart';

import '../../../../tool/fake_esp32.dart';

const _key = 'test-ble-secret-key';
const _deviceId = '38:3E:51:6F:C7:A8';

class MockKeyRepository extends Mock implements BleKeyRepository {}

class _FakeLink implements BleLink {
  _FakeLink(this.esp32);

  final FakeEsp32 esp32;
  final Completer<void> _disconnected = Completer<void>();
  bool bondFails = false;

  @override
  BleTransport get transport => esp32;

  @override
  Future<void> get disconnected => _disconnected.future;

  @override
  Future<void> ensureBonded() async {
    if (bondFails) throw const BleConnectException('Ghép nối thất bại.');
    esp32.isBonded = true;
  }

  @override
  Future<void> disconnect() async {
    if (!_disconnected.isCompleted) _disconnected.complete();
  }

  /// ESP32 tắt hoặc ra khỏi tầm.
  Future<void> drop() async {
    await esp32.disconnect();
    if (!_disconnected.isCompleted) _disconnected.complete();
  }
}

class _FakeConnector implements BleConnector {
  _FakeConnector(this.link);

  final _FakeLink link;
  BleConnectException? error;
  final List<String> requested = [];

  @override
  Future<BleLink> connect(String deviceId) async {
    requested.add(deviceId);
    if (error != null) throw error!;
    return link;
  }
}

void main() {
  final now = DateTime.utc(2026, 11, 8, 3);
  late FakeEsp32 esp32;
  late _FakeLink link;
  late _FakeConnector connector;
  late MockKeyRepository keys;
  late BleSessionCubit cubit;

  setUp(() {
    esp32 = FakeEsp32(secretKey: _key)..isBonded = false;
    link = _FakeLink(esp32);
    connector = _FakeConnector(link);
    keys = MockKeyRepository();
    when(() => keys.getKey(_deviceId)).thenAnswer((_) async => _key);
    cubit = BleSessionCubit(connector: connector, keyRepository: keys, now: () => now);
  });

  tearDown(() => cubit.close());

  group('Kết nối', () {
    test('kết nối được thì đọc cảm biến và đặt giờ cho ESP32', () async {
      await cubit.connect(_deviceId);
      await pumpEventQueue();

      expect(cubit.state.isConnectedTo(_deviceId), isTrue);
      expect(cubit.state.sample?.waterLevelPercent, 60.0);
      expect(esp32.syncedUnixSeconds, now.millisecondsSinceEpoch ~/ 1000);
    });

    test('chưa có khóa thì không kết nối và báo cách lấy khóa', () async {
      when(() => keys.getKey(_deviceId)).thenAnswer((_) async => null);

      await cubit.connect(_deviceId);

      expect(cubit.state.status, BleSessionStatus.disconnected);
      expect(cubit.state.errorMessage, contains('Chưa có khóa Bluetooth'));
      expect(connector.requested, isEmpty);
    });

    test('không tìm thấy thiết bị thì báo lỗi', () async {
      connector.error = const BleConnectException('Không tìm thấy thiết bị.');

      await cubit.connect(_deviceId);

      expect(cubit.state.status, BleSessionStatus.disconnected);
      expect(cubit.state.errorMessage, 'Không tìm thấy thiết bị.');
    });

    test('nhận gói cảm biến mới qua Notify', () async {
      await cubit.connect(_deviceId);
      await pumpEventQueue();

      esp32.isPumpOn = true;
      esp32.emitSensorPacket();
      await pumpEventQueue();

      expect(cubit.state.sample?.isPumpOn, isTrue);
    });

    test('mất kết nối thì chuyển về chưa kết nối và báo lý do', () async {
      await cubit.connect(_deviceId);
      await pumpEventQueue();

      await link.drop();
      await pumpEventQueue();

      expect(cubit.state.status, BleSessionStatus.disconnected);
      expect(cubit.state.errorMessage, 'Mất kết nối Bluetooth với thiết bị.');
    });
  });

  group('Bơm', () {
    setUp(() async {
      await cubit.connect(_deviceId);
      await pumpEventQueue();
    });

    test('bật bơm 5 giây', () async {
      expect(await cubit.setPump(true), isNull);
      expect(esp32.isPumpOn, isTrue);
      expect(await cubit.setPump(false), isNull);
      expect(esp32.isPumpOn, isFalse);
    });

    test('bồn cạn thì báo bằng tiếng Việt', () async {
      esp32.waterPercent = 5;
      expect(await cubit.setPump(true), 'Bồn nước gần cạn, thiết bị không bơm.');
    });

    test('đang tự động thì báo lệnh bị từ chối', () async {
      esp32.isAutoMode = true;
      expect(await cubit.setPump(true), contains('Thiết bị không thực hiện lệnh'));
    });
  });

  test('chưa kết nối thì không gửi lệnh bơm', () async {
    expect(await cubit.setPump(true), 'Chưa kết nối Bluetooth với thiết bị.');
  });

  group('Wi-Fi', () {
    test('tự kết nối, ghép nối rồi gửi Wi-Fi', () async {
      final error = await cubit.sendWifi(_deviceId, ssid: 'VuonNha', password: 'matkhau123');

      expect(error, isNull);
      expect(esp32.isBonded, isTrue);
      expect(esp32.savedSsid, 'VuonNha');
    });

    test('ghép nối thất bại thì không gửi', () async {
      link.bondFails = true;

      final error = await cubit.sendWifi(_deviceId, ssid: 'VuonNha', password: 'matkhau123');

      expect(error, 'Ghép nối thất bại.');
      expect(esp32.savedSsid, isNull);
    });

    test('ESP32 không vào được Wi-Fi', () async {
      esp32.wifiConnects = false;

      final error = await cubit.sendWifi(_deviceId, ssid: 'VuonNha', password: 'matkhau123');

      expect(error, 'Thiết bị không vào được Wi-Fi. Kiểm tra tên và mật khẩu.');
    });

    test('mật khẩu sai định dạng thì không kết nối', () async {
      final error = await cubit.sendWifi(_deviceId, ssid: 'VuonNha', password: 'ngan');

      expect(error, 'Mật khẩu Wi-Fi phải từ 8 đến 63 byte');
      expect(connector.requested, isEmpty);
    });
  });
}
