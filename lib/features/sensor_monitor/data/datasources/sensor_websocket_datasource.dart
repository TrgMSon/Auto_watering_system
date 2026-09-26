import 'dart:async';
import '../../../../core/network/websocket_client.dart';
import '../models/sensor_reading_model.dart';

class SensorWebSocketDataSource {
  final WebSocketClient wsClient;
  final _controller = StreamController<SensorReadingModel>.broadcast();

  SensorWebSocketDataSource({required this.wsClient});

  Stream<SensorReadingModel> get sensorStream => _controller.stream;

  void startListening() {
    wsClient.subscribeSensorTelemetry(
      topic: '/topic/sensors/realtime',
      onData: (data) {
        final reading = SensorReadingModel.fromJson(data);
        _controller.add(reading);
      },
    );
  }

  void stopListening() {
    wsClient.unsubscribe('/topic/sensors/realtime');
  }

  void dispose() {
    stopListening();
    _controller.close();
  }
}
