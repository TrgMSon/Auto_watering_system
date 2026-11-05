import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:stomp_dart_client/stomp_dart_client.dart';
import 'package:watering_app/core/constants/api_endpoints.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/core/network/realtime_client.dart';

class MockApiClient extends Mock implements ApiClient {}

class MockStompClient extends Mock implements StompClient {}

void main() {
  late MockApiClient apiClient;
  late List<StompConfig> configs;
  late List<MockStompClient> clients;
  late Map<String, void Function(StompFrame)> subscriptions;
  late RealtimeClient realtime;

  StompFrame frame(String body) =>
      StompFrame(command: 'MESSAGE', headers: const {}, body: body);

  setUp(() {
    apiClient = MockApiClient();
    configs = [];
    clients = [];
    subscriptions = {};
    when(() => apiClient.accessToken).thenAnswer((_) async => 'token-1');
    when(() => apiClient.refreshSession()).thenAnswer((_) async {
      when(() => apiClient.accessToken).thenAnswer((_) async => 'token-2');
      return true;
    });
    realtime = RealtimeClient(
      apiClient,
      url: 'wss://example.test/ws/websocket',
      reconnectDelay: Duration.zero,
      clientFactory: (config) {
        final client = MockStompClient();
        when(client.activate).thenReturn(null);
        when(client.deactivate).thenReturn(null);
        when(() => client.subscribe(
              destination: any(named: 'destination'),
              callback: any(named: 'callback'),
            )).thenAnswer((invocation) {
          final destination = invocation.namedArguments[#destination] as String;
          subscriptions[destination] =
              invocation.namedArguments[#callback] as void Function(StompFrame);
          return ({Map<String, String>? unsubscribeHeaders}) {
            subscriptions.remove(destination);
          };
        });
        configs.add(config);
        clients.add(client);
        return client;
      },
    );
  });

  tearDown(() => realtime.dispose());

  Future<void> connectAndOpen() async {
    await realtime.connect();
    configs.last.onConnect(frame(''));
  }

  group('ApiEndpoints.webSocketUrlFor', () {
    test('turns an https API address into the secure WebSocket address', () {
      expect(
        ApiEndpoints.webSocketUrlFor('https://example.onrender.com/api'),
        'wss://example.onrender.com/ws/websocket',
      );
    });

    test('keeps the port of a local http address', () {
      expect(
        ApiEndpoints.webSocketUrlFor('http://10.0.2.2:8080/api'),
        'ws://10.0.2.2:8080/ws/websocket',
      );
    });
  });

  test('connects with the access token and subscribes to personal queues', () async {
    await connectAndOpen();

    expect(configs.single.stompConnectHeaders, {'Authorization': 'Bearer token-1'});
    expect(configs.single.url, 'wss://example.test/ws/websocket');
    expect(
      subscriptions.keys,
      containsAll(['/users/queue/notifications', '/users/queue/pump-status']),
    );
  });

  test('does not connect without a token', () async {
    when(() => apiClient.accessToken).thenAnswer((_) async => null);

    await realtime.connect();

    expect(configs, isEmpty);
  });

  test('publishes device_shared notifications and ignores malformed ones', () async {
    await connectAndOpen();
    final events = <DeviceSharedEvent>[];
    final sub = realtime.deviceShared.listen(events.add);

    final callback = subscriptions['/users/queue/notifications']!;
    callback(frame('not json'));
    callback(frame('{"type":"other"}'));
    callback(frame('{"type":"device_shared","deviceId":"A","message":"Đã chia sẻ"}'));
    await pumpEventQueue();

    expect(events.single.deviceId, 'A');
    expect(events.single.message, 'Đã chia sẻ');
    await sub.cancel();
  });

  test('publishes pump status and ignores unknown values', () async {
    await connectAndOpen();
    final events = <bool>[];
    final sub = realtime.pumpStatus.listen(events.add);

    final callback = subscriptions['/users/queue/pump-status']!;
    callback(frame('ON'));
    callback(frame('"OFF"'));
    callback(frame('UNKNOWN'));
    await pumpEventQueue();

    expect(events, [true, false]);
    await sub.cancel();
  });

  test('switches the device alert subscription with the selected device', () async {
    await connectAndOpen();
    final events = <DeviceAlertEvent>[];
    final sub = realtime.deviceAlerts.listen(events.add);

    realtime.watchDevice('A');
    subscriptions['/topic/notification/A']!(frame('Thiết bị lỗi'));
    await pumpEventQueue();
    realtime.watchDevice('B');

    expect(events.single.deviceId, 'A');
    expect(subscriptions.containsKey('/topic/notification/A'), isFalse);
    expect(subscriptions.containsKey('/topic/notification/B'), isTrue);
    await sub.cancel();
  });

  test('subscribes to a device chosen before the connection opened', () async {
    realtime.watchDevice('A');

    await connectAndOpen();

    expect(subscriptions.containsKey('/topic/notification/A'), isTrue);
  });

  test('refreshes the token and reconnects after the server rejects it', () async {
    await realtime.connect();

    configs.first.onStompError(frame('invalid'));
    await Future<void>.delayed(const Duration(milliseconds: 5));

    verify(() => apiClient.refreshSession()).called(1);
    verify(clients.first.deactivate).called(1);
    expect(configs.last.stompConnectHeaders, {'Authorization': 'Bearer token-2'});
    verify(clients.last.activate).called(1);
  });

  test('reconnects without refreshing after the network drops', () async {
    await connectAndOpen();

    configs.first.onWebSocketDone();
    await Future<void>.delayed(const Duration(milliseconds: 5));

    verifyNever(() => apiClient.refreshSession());
    expect(configs, hasLength(2));
  });

  test('does not reconnect after disconnect', () async {
    await connectAndOpen();

    realtime.disconnect();
    configs.first.onWebSocketDone();
    await Future<void>.delayed(const Duration(milliseconds: 5));

    verify(clients.first.deactivate).called(1);
    expect(configs, hasLength(1));
  });
}
