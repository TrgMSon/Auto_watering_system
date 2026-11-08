import 'dart:async';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../domain/ble_command_service.dart';
import '../domain/ble_connector.dart';
import '../domain/ble_protocol.dart';
import '../domain/ble_transport.dart';

/// Kết nối BLE thật qua `flutter_blue_plus` (Android).
class FlutterBluePlusConnector implements BleConnector {
  static const Duration _scanTimeout = Duration(seconds: 10);
  static const Duration _connectTimeout = Duration(seconds: 15);

  /// Firmware cần MTU đủ lớn cho gói kết quả `0005` và gói cấu hình (tối đa 90 byte).
  static const int _mtu = 185;

  @override
  Future<BleLink> connect(String deviceId) async {
    await _ensureAdapterOn();
    final device = await _scan(BleDeviceId.toAdvertisingName(deviceId));
    try {
      await device.connect(license: License.nonprofit, timeout: _connectTimeout, mtu: _mtu);
      final link = await _FlutterBluePlusLink.open(device);
      return link;
    } on BleConnectException {
      await device.disconnect();
      rethrow;
    } catch (_) {
      await device.disconnect();
      throw const BleConnectException('Không kết nối được với thiết bị. Hãy lại gần hơn rồi thử lại.');
    }
  }

  Future<void> _ensureAdapterOn() async {
    if (!await FlutterBluePlus.isSupported) {
      throw const BleConnectException('Điện thoại không hỗ trợ Bluetooth Low Energy.');
    }
    final state = await FlutterBluePlus.adapterState
        .where((s) => s != BluetoothAdapterState.unknown)
        .first
        .timeout(const Duration(seconds: 3), onTimeout: () => BluetoothAdapterState.unknown);
    if (state == BluetoothAdapterState.unauthorized) {
      throw const BleConnectException('Ứng dụng chưa được cấp quyền Bluetooth.');
    }
    if (state != BluetoothAdapterState.on) {
      throw const BleConnectException('Hãy bật Bluetooth rồi thử lại.');
    }
  }

  Future<BluetoothDevice> _scan(String advertisingName) async {
    final found = Completer<BluetoothDevice>();
    final subscription = FlutterBluePlus.onScanResults.listen((results) {
      for (final result in results) {
        final name = result.advertisementData.advName.isNotEmpty
            ? result.advertisementData.advName
            : result.device.platformName;
        if (name == advertisingName && !found.isCompleted) found.complete(result.device);
      }
    });
    try {
      await FlutterBluePlus.startScan(
        withServices: [Guid(BleUuids.service)],
        timeout: _scanTimeout,
      );
      return await found.future.timeout(_scanTimeout);
    } on TimeoutException {
      throw const BleConnectException('Không tìm thấy thiết bị. Hãy lại gần thiết bị và kiểm tra thiết bị đang bật.');
    } finally {
      await subscription.cancel();
      await FlutterBluePlus.stopScan();
    }
  }
}

class _FlutterBluePlusLink implements BleLink {
  _FlutterBluePlusLink(this._device, Map<String, BluetoothCharacteristic> characteristics)
      : transport = _FlutterBluePlusTransport(_device, characteristics) {
    _connectionSubscription = _device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) _markDisconnected();
    });
  }

  static Future<_FlutterBluePlusLink> open(BluetoothDevice device) async {
    final services = await device.discoverServices();
    final service = services.where((s) => s.uuid == Guid(BleUuids.service)).firstOrNull;
    if (service == null) {
      throw const BleConnectException('Thiết bị không có dịch vụ BLE của hệ thống tưới.');
    }
    final characteristics = {
      for (final c in service.characteristics) c.uuid.str128: c,
    };
    return _FlutterBluePlusLink(device, characteristics);
  }

  final BluetoothDevice _device;
  final Completer<void> _disconnected = Completer<void>();
  late final StreamSubscription<BluetoothConnectionState> _connectionSubscription;

  @override
  final _FlutterBluePlusTransport transport;

  @override
  Future<void> get disconnected => _disconnected.future;

  @override
  Future<void> ensureBonded() async {
    try {
      await _device.createBond();
    } catch (_) {
      throw const BleConnectException(
        'Ghép nối thất bại. Cần mở cửa sổ ghép nối trên thiết bị (lệnh BLE_PAIR qua cổng USB) rồi thử lại trong 60 giây.',
      );
    }
  }

  @override
  Future<void> disconnect() async {
    await _device.disconnect();
    _markDisconnected();
  }

  void _markDisconnected() {
    if (_disconnected.isCompleted) return;
    _disconnected.complete();
    _connectionSubscription.cancel();
    transport.close();
  }
}

class _FlutterBluePlusTransport implements BleTransport {
  _FlutterBluePlusTransport(this._device, this._characteristics);

  final BluetoothDevice _device;
  final Map<String, BluetoothCharacteristic> _characteristics;
  final List<StreamController<List<int>>> _streams = [];

  @override
  int get maxWritePayloadBytes => _device.mtuNow - 3;

  @override
  Future<List<int>> read(String characteristicUuid) => _find(characteristicUuid).read();

  @override
  Future<void> write(String characteristicUuid, List<int> value) async {
    try {
      await _find(characteristicUuid).write(value);
    } on FlutterBluePlusException {
      if (_device.isDisconnected) throw const BleDisconnectedException();
      throw const BleWriteRejectedException();
    }
  }

  @override
  Future<Stream<List<int>>> subscribe(String characteristicUuid) async {
    final characteristic = _find(characteristicUuid);
    // Đăng ký nghe trước khi bật Notify để không lỡ gói đầu tiên.
    final controller = StreamController<List<int>>.broadcast();
    final subscription = characteristic.onValueReceived.listen(controller.add);
    controller.onCancel = subscription.cancel;
    _streams.add(controller);
    await characteristic.setNotifyValue(true);
    return controller.stream;
  }

  /// Đóng mọi luồng Notify để bên nghe biết đã mất kết nối.
  void close() {
    for (final controller in _streams) {
      controller.close();
    }
    _streams.clear();
  }

  BluetoothCharacteristic _find(String uuid) {
    final characteristic = _characteristics[uuid.toLowerCase()];
    if (characteristic == null) {
      throw BleConnectException('Thiết bị thiếu characteristic $uuid; có thể firmware đã cũ.');
    }
    return characteristic;
  }
}
