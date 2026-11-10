import 'dart:async';

import 'package:equatable/equatable.dart';
import 'package:flutter_bloc/flutter_bloc.dart';

import '../../../../core/network/api_client.dart';
import '../../data/ble_key_repository.dart';
import '../../domain/ble_command_service.dart';
import '../../domain/ble_commands.dart';
import '../../domain/ble_connector.dart';
import '../../domain/ble_messages.dart';
import '../../domain/ble_protocol.dart';

enum BleSessionStatus { disconnected, connecting, connected }

class BleSessionState extends Equatable {
  const BleSessionState({
    this.status = BleSessionStatus.disconnected,
    this.deviceId,
    this.sample,
    this.errorMessage,
    this.isSending = false,
  });

  final BleSessionStatus status;

  /// Mã thiết bị (MAC có dấu `:`) đang hoặc sắp kết nối.
  final String? deviceId;
  final BleSensorSample? sample;
  final String? errorMessage;

  /// Đang chờ ESP32 trả kết quả một lệnh.
  final bool isSending;

  bool isConnectedTo(String deviceId) =>
      status == BleSessionStatus.connected && this.deviceId == deviceId;

  @override
  List<Object?> get props => [status, deviceId, sample, errorMessage, isSending];
}

/// Giữ một kết nối BLE tới ESP32 và gửi lệnh qua đó.
/// Theo quy ước nhóm: thiết bị online thì điều khiển qua server, offline thì qua BLE.
class BleSessionCubit extends Cubit<BleSessionState> {
  BleSessionCubit({
    required BleConnector connector,
    required BleKeyRepository keyRepository,
    DateTime Function()? now,
  })  : _connector = connector,
        _keyRepository = keyRepository,
        _now = now ?? DateTime.now,
        super(const BleSessionState());

  /// Firmware chỉ cho bơm tay tối đa 5 giây mỗi lần.
  static const int pumpRunSeconds = 5;

  final BleConnector _connector;
  final BleKeyRepository _keyRepository;
  final DateTime Function() _now;

  BleLink? _link;
  BleCommandService? _service;
  StreamSubscription<BleSensorSample>? _samples;
  Future<void> _lastCommand = Future.value();

  Future<void> connect(String deviceId) async {
    if (state.isConnectedTo(deviceId) || state.status == BleSessionStatus.connecting) return;
    await disconnect();
    emit(BleSessionState(status: BleSessionStatus.connecting, deviceId: deviceId));
    try {
      final key = await _loadKey(deviceId);
      final link = await _connector.connect(deviceId);
      if (isClosed) {
        await link.disconnect();
        return;
      }
      _attach(link, key, deviceId);
    } on BleConnectException catch (e) {
      _fail(deviceId, e.message);
    } on ApiException catch (e) {
      _fail(deviceId, e.message);
    }
  }

  Future<void> disconnect() async {
    final link = _link;
    _detach();
    if (link != null) await link.disconnect();
    if (!isClosed && state.status != BleSessionStatus.disconnected) {
      emit(const BleSessionState());
    }
  }

  /// Trả về null khi thành công, hoặc thông báo lỗi để hiện cho người dùng.
  Future<String?> setPump(bool turnOn) async {
    final BleCommand command = turnOn ? BlePumpOnCommand(pumpRunSeconds) : const BlePumpOffCommand();
    final result = await _send(command);
    return result.$2 ?? (result.$1!.isSuccess ? null : resultMessage(result.$1!.code));
  }

  /// Đổi chế độ tưới cục bộ qua BLE, không cần backend hoặc MQTT.
  Future<String?> setAutoMode(bool enabled) async {
    final result = await _send(BleAutoModeCommand(enabled));
    return result.$2 ?? (result.$1!.isSuccess ? null : resultMessage(result.$1!.code));
  }

  /// Kết nối (nếu cần), ghép nối rồi gửi Wi-Fi. Trả về null khi ESP32 vào được Wi-Fi.
  Future<String?> sendWifi(String deviceId, {required String ssid, required String password}) async {
    final BleWifiCommand command;
    try {
      command = BleWifiCommand(ssid: ssid, password: password);
    } on ArgumentError catch (e) {
      return '${e.message}';
    }
    await connect(deviceId);
    final link = _link;
    if (link == null || !state.isConnectedTo(deviceId)) {
      return state.errorMessage ?? 'Không kết nối được với thiết bị.';
    }
    try {
      await link.ensureBonded();
    } on BleConnectException catch (e) {
      return e.message;
    }
    final result = await _send(command);
    if (result.$2 != null) return result.$2;
    return result.$1!.code == BleResultCode.wifiOk ? null : resultMessage(result.$1!.code);
  }

  /// Gửi một lệnh; trả về kết quả hoặc thông báo lỗi.
  /// Firmware chỉ xử lý một lệnh mỗi lúc, nên lệnh sau chờ lệnh trước xong
  /// (ví dụ lệnh đặt giờ tự gửi ngay sau khi kết nối).
  Future<(BleCommandResult?, String?)> _send(BleCommand command) async {
    final previous = _lastCommand;
    final done = Completer<void>();
    _lastCommand = done.future;
    try {
      await previous;
      final service = _service;
      if (service == null) return (null, 'Chưa kết nối Bluetooth với thiết bị.');
      if (!isClosed) emit(_copy(isSending: true));
      try {
        return (await service.send(command), null);
      } on BleCommandException catch (e) {
        return (null, e.message);
      } finally {
        if (!isClosed && _service == service) emit(_copy(isSending: false));
      }
    } finally {
      done.complete();
    }
  }

  Future<String> _loadKey(String deviceId) async {
    final key = await _keyRepository.getKey(deviceId);
    if (key == null) {
      throw const BleConnectException(
        'Chưa có khóa Bluetooth của thiết bị. Cần kết nối mạng một lần để tải khóa, hoặc bạn không còn quyền với thiết bị.',
      );
    }
    return key;
  }

  void _attach(BleLink link, String key, String deviceId) {
    _link = link;
    final service = BleCommandService(transport: link.transport, secretKey: key);
    _service = service;
    emit(BleSessionState(status: BleSessionStatus.connected, deviceId: deviceId));

    _samples = service.watchSensors().listen((sample) {
      if (_service == service) emit(_copy(sample: sample));
    });
    unawaited(link.disconnected.then((_) {
      if (_link != link || isClosed) return;
      _detach();
      emit(BleSessionState(deviceId: deviceId, errorMessage: 'Mất kết nối Bluetooth với thiết bị.'));
    }));
    unawaited(_syncClock(service));
  }

  /// ESP32 không có pin đồng hồ, nên đặt lại giờ mỗi lần kết nối. Lỗi không chặn việc khác.
  Future<void> _syncClock(BleCommandService service) async {
    try {
      final sample = await service.readSensors();
      if (_service == service && !isClosed) emit(_copy(sample: sample));
    } on Object {
      // Gói Notify 2 giây sau sẽ cập nhật.
    }
    if (_service != service || isClosed) return;
    await _send(BleTimeSyncCommand.fromDateTime(_now()));
  }

  void _detach() {
    _samples?.cancel();
    _samples = null;
    _service = null;
    _link = null;
  }

  void _fail(String deviceId, String message) {
    _detach();
    if (!isClosed) emit(BleSessionState(deviceId: deviceId, errorMessage: message));
  }

  BleSessionState _copy({BleSensorSample? sample, bool? isSending}) => BleSessionState(
        status: state.status,
        deviceId: state.deviceId,
        sample: sample ?? state.sample,
        isSending: isSending ?? state.isSending,
      );

  @override
  Future<void> close() async {
    await disconnect();
    return super.close();
  }
}

/// Thông báo tiếng Việt cho mã kết quả từ ESP32.
String resultMessage(BleResultCode code) => switch (code) {
      BleResultCode.ok || BleResultCode.wifiOk => 'Thành công.',
      BleResultCode.badSig => 'Khóa Bluetooth không khớp với thiết bị. Hãy kết nối mạng để tải lại khóa.',
      BleResultCode.nonceExpired => 'Lệnh hết hạn. Vui lòng thử lại.',
      BleResultCode.badJson => 'Thiết bị không hiểu lệnh. Có thể firmware khác phiên bản.',
      BleResultCode.outOfRange => 'Giá trị nằm ngoài khoảng thiết bị cho phép.',
      BleResultCode.lowWater => 'Bồn nước gần cạn, thiết bị không bơm.',
      BleResultCode.maintenance => 'Thiết bị đang bảo trì.',
      BleResultCode.sensorError => 'Cảm biến lỗi nên thiết bị không bơm.',
      BleResultCode.pumpBusy => 'Bơm đang chạy. Hãy tắt bơm trước.',
      BleResultCode.invalidTime => 'Giờ trên điện thoại không hợp lệ.',
      BleResultCode.wifiFail => 'Thiết bị không vào được Wi-Fi. Kiểm tra tên và mật khẩu.',
      BleResultCode.storageError => 'Thiết bị không lưu được cài đặt. Vui lòng thử lại.',
      BleResultCode.unknown => 'Thiết bị trả kết quả không xác định.',
    };
