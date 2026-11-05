import 'dart:async';
import 'dart:convert';

import 'package:stomp_dart_client/stomp_dart_client.dart';

import '../constants/api_endpoints.dart';
import 'api_client.dart';

class DeviceSharedEvent {
  final String deviceId;
  final String message;

  const DeviceSharedEvent({required this.deviceId, required this.message});
}

class DeviceAlertEvent {
  final String deviceId;
  final String message;

  const DeviceAlertEvent({required this.deviceId, required this.message});
}

/// Nhận thông báo realtime từ backend qua STOMP và tự kết nối lại khi mất mạng.
///
/// Đây chỉ là kênh bổ sung: thông báo gửi lúc app đang offline sẽ không được gửi lại,
/// nên dữ liệu chính vẫn lấy qua REST.
class RealtimeClient {
  RealtimeClient(
    this._apiClient, {
    StompClient Function(StompConfig config)? clientFactory,
    Duration reconnectDelay = const Duration(seconds: 5),
    String? url,
  })  : _clientFactory = clientFactory ?? ((config) => StompClient(config: config)),
        _reconnectDelay = reconnectDelay,
        _url = url ?? ApiEndpoints.webSocketUrl;

  final ApiClient _apiClient;
  final StompClient Function(StompConfig config) _clientFactory;
  final Duration _reconnectDelay;
  final String _url;

  final _deviceShared = StreamController<DeviceSharedEvent>.broadcast();
  final _pumpStatus = StreamController<bool>.broadcast();
  final _deviceAlerts = StreamController<DeviceAlertEvent>.broadcast();

  StompClient? _client;
  Timer? _reconnectTimer;
  void Function()? _unsubscribeDevice;
  String? _watchedDeviceId;
  bool _isWanted = false;
  bool _isConnected = false;
  bool _tokenRejected = false;

  Stream<DeviceSharedEvent> get deviceShared => _deviceShared.stream;

  /// Backend chỉ gửi ON/OFF, không kèm mã thiết bị.
  Stream<bool> get pumpStatus => _pumpStatus.stream;

  Stream<DeviceAlertEvent> get deviceAlerts => _deviceAlerts.stream;

  Future<void> connect() async {
    _isWanted = true;
    if (_client != null || _reconnectTimer != null) return;
    await _open();
  }

  void disconnect() {
    _isWanted = false;
    _tokenRejected = false;
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
    _closeClient();
  }

  void watchDevice(String? deviceId) {
    if (deviceId == _watchedDeviceId) return;
    _watchedDeviceId = deviceId;
    _subscribeToWatchedDevice();
  }

  Future<void> _open() async {
    final token = await _apiClient.accessToken;
    if (token == null || !_isWanted || _client != null) return;

    late final StompClient client;
    client = _clientFactory(StompConfig(
      url: _url,
      // Tự lên lịch kết nối lại để mỗi lần dùng access token mới nhất.
      reconnectDelay: Duration.zero,
      stompConnectHeaders: {'Authorization': 'Bearer $token'},
      onConnect: (_) {
        if (identical(client, _client)) _onConnected(client);
      },
      onStompError: (_) {
        if (!identical(client, _client)) return;
        _tokenRejected = true;
        _scheduleReconnect();
      },
      onWebSocketError: (_) {
        if (identical(client, _client)) _scheduleReconnect();
      },
      onWebSocketDone: () {
        if (identical(client, _client)) _scheduleReconnect();
      },
    ));
    _client = client;
    client.activate();
  }

  void _onConnected(StompClient client) {
    _isConnected = true;
    _tokenRejected = false;
    client.subscribe(
      destination: '/users/queue/notifications',
      callback: (frame) => _onNotification(frame.body),
    );
    client.subscribe(
      destination: '/users/queue/pump-status',
      callback: (frame) => _onPumpStatus(frame.body),
    );
    _subscribeToWatchedDevice();
  }

  void _subscribeToWatchedDevice() {
    _unsubscribeDevice?.call();
    _unsubscribeDevice = null;
    final client = _client;
    final deviceId = _watchedDeviceId;
    if (client == null || deviceId == null || !_isConnected) return;
    _unsubscribeDevice = client.subscribe(
      destination: '/topic/notification/$deviceId',
      callback: (frame) {
        final message = frame.body;
        if (message != null && message.isNotEmpty) {
          _deviceAlerts.add(DeviceAlertEvent(deviceId: deviceId, message: message));
        }
      },
    );
  }

  void _onNotification(String? body) {
    if (body == null) return;
    try {
      final json = jsonDecode(body);
      if (json is Map<String, dynamic> && json['type'] == 'device_shared') {
        _deviceShared.add(DeviceSharedEvent(
          deviceId: json['deviceId']?.toString() ?? '',
          message: json['message']?.toString() ?? 'Có thiết bị mới được chia sẻ với bạn.',
        ));
      }
    } on FormatException {
      // Bỏ qua thông báo sai định dạng để không làm hỏng kết nối.
    }
  }

  void _onPumpStatus(String? body) {
    final status = body?.replaceAll('"', '').trim().toUpperCase();
    if (status == 'ON') _pumpStatus.add(true);
    if (status == 'OFF') _pumpStatus.add(false);
  }

  void _scheduleReconnect() {
    _closeClient();
    if (!_isWanted || _reconnectTimer != null) return;
    _reconnectTimer = Timer(_reconnectDelay, () async {
      _reconnectTimer = null;
      if (_tokenRejected) await _apiClient.refreshSession();
      await _open();
    });
  }

  void _closeClient() {
    final client = _client;
    _client = null;
    _isConnected = false;
    _unsubscribeDevice = null;
    client?.deactivate();
  }

  void dispose() {
    disconnect();
    _deviceShared.close();
    _pumpStatus.close();
    _deviceAlerts.close();
  }
}
