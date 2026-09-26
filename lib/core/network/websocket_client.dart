import 'dart:convert';
import 'package:stomp_dart_client/stomp_dart_client.dart';
import '../constants/api_endpoints.dart';

typedef TelemetryCallback = void Function(Map<String, dynamic> data);
typedef VoidCallback = void Function();

class WebSocketClient {
  StompClient? _stompClient;
  final Map<String, StompUnsubscribe?> _subscriptions = {};
  bool _isConnected = false;

  bool get isConnected => _isConnected;

  void connect({
    required String jwtToken,
    required VoidCallback onConnected,
    required void Function(String error) onError,
  }) {
    _stompClient = StompClient(
      config: StompConfig(
        url: ApiEndpoints.wsUrl,
        onConnect: (StompFrame frame) {
          _isConnected = true;
          onConnected();
        },
        onDisconnect: (StompFrame frame) {
          _isConnected = false;
        },
        onWebSocketError: (dynamic error) {
          _isConnected = false;
          onError(error.toString());
        },
        stompConnectHeaders: {'Authorization': 'Bearer $jwtToken'},
        webSocketConnectHeaders: {'Authorization': 'Bearer $jwtToken'},
        reconnectDelay: const Duration(seconds: 5),
      ),
    );
    _stompClient?.activate();
  }

  void subscribeSensorTelemetry({
    required String topic,
    required TelemetryCallback onData,
  }) {
    final unsubscribe = _stompClient?.subscribe(
      destination: topic,
      callback: (StompFrame frame) {
        if (frame.body != null) {
          final data = jsonDecode(frame.body!) as Map<String, dynamic>;
          onData(data);
        }
      },
    );
    _subscriptions[topic] = unsubscribe;
  }

  void sendCommand({
    required String destination,
    required Map<String, dynamic> payload,
  }) {
    _stompClient?.send(
      destination: destination,
      body: jsonEncode(payload),
    );
  }

  void unsubscribe(String topic) {
    _subscriptions[topic]?.call(unsubscribeHeaders: {});
    _subscriptions.remove(topic);
  }

  void disconnect() {
    _subscriptions.clear();
    _stompClient?.deactivate();
    _isConnected = false;
  }
}
