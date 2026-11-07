// Backend giả để test app, không cần DB, MQTT hay ESP thật.
// Chạy trong thư mục mobile: dart tool/fake_backend.dart [port] [--lan]
// Mặc định chỉ máy này và máy ảo gọi được; thêm --lan để điện thoại cùng Wi-Fi gọi được.
// Dữ liệu chỉ nằm trong bộ nhớ và mất khi tắt script.
import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:math';

const String testPassword = '123456';

const Duration _accessTokenLifetime = Duration(hours: 1);
const Duration _refreshTokenLifetime = Duration(days: 7);
const Duration _historyStep = Duration(minutes: 15);
const Duration _telemetryStep = Duration(seconds: 5);
const Duration _pumpAckDelay = Duration(seconds: 2);

const String _commandHelp = '''
Commands:
  status       show devices and session generations
  offline <n>  mark device n offline (n = 1..5)
  online <n>   mark device n online
  expire       reject current access tokens (refresh still works)
  revoke       reject access and refresh tokens (forces logout)
  down         stop answering requests, like losing the server
  up           start answering requests again
  help         show this list
  quit         stop the fake backend''';

Future<void> main(List<String> args) async {
  final allowLan = args.contains('--lan');
  final portArgs = args.where((arg) => arg != '--lan');
  final port = portArgs.isEmpty ? 8081 : int.parse(portArgs.first);
  final backend = FakeBackend(logRequests: true, allowLan: allowLan);
  await backend.start(port: port);

  stdout.writeln('Fake backend: http://127.0.0.1:${backend.port}/api');
  if (allowLan) await _printLanAddresses(backend.port);
  stdout.writeln(
    'Accounts (password $testPassword): user@test.com, empty@test.com, admin@test.com',
  );
  stdout.writeln(_commandHelp);

  final lines = stdin.transform(utf8.decoder).transform(const LineSplitter());
  await for (final line in lines) {
    if (line.trim() == 'quit') break;
    stdout.writeln(await backend.runCommand(line));
  }
  await backend.stop();
  exit(0);
}

Future<void> _printLanAddresses(int port) async {
  final interfaces = await NetworkInterface.list(type: InternetAddressType.IPv4);
  for (final interface in interfaces) {
    for (final address in interface.addresses) {
      stdout.writeln('  LAN (${interface.name}): http://${address.address}:$port/api');
    }
  }
}

enum _PumpBehavior { succeeds, rejects, noResponse }

class _Device {
  _Device({
    required this.id,
    required this.name,
    required this.behavior,
    this.hasData = true,
    this.lastSeen,
  });

  final String id;
  final String name;
  final _PumpBehavior behavior;
  final bool hasData;

  /// Thời điểm cuối còn kết nối; null nghĩa là thiết bị đang online.
  DateTime? lastSeen;

  bool get isOnline => lastSeen == null;
}

class _Account {
  const _Account(this.id, this.email, this.role, {this.hasDevices = true});

  final int id;
  final String email;
  final String role;
  final bool hasDevices;
}

class _Command {
  const _Command(this.device, this.createdAt);

  final _Device device;
  final DateTime createdAt;
}

class _Reply {
  const _Reply(this.status, this.message, [this.data]);

  final int status;
  final String message;
  final Object? data;
}

class FakeBackend {
  FakeBackend({this.logRequests = false, this.allowLan = false});

  final bool logRequests;
  final bool allowLan;

  static const List<_Account> _accounts = [
    _Account(1, 'user@test.com', 'USER'),
    _Account(2, 'empty@test.com', 'USER', hasDevices: false),
    _Account(3, 'admin@test.com', 'ADMIN'),
  ];

  final Map<String, _Account> _registeredAccounts = {};
  final Map<String, String> _registeredPasswords = {};

  final List<_Device> _devices = [
    _Device(
        id: 'ESP_01',
        name: 'Vườn 1 - Bơm thành công',
        behavior: _PumpBehavior.succeeds),
    _Device(
      id: 'ESP_02',
      name: 'Vườn 2 - Offline',
      behavior: _PumpBehavior.succeeds,
      lastSeen: DateTime.now().subtract(const Duration(hours: 2)),
    ),
    _Device(
        id: 'ESP_03',
        name: 'Vườn 3 - Bơm bị từ chối',
        behavior: _PumpBehavior.rejects),
    _Device(
      id: 'ESP_04',
      name: 'Vườn 4 - Chưa có dữ liệu',
      behavior: _PumpBehavior.succeeds,
      hasData: false,
    ),
    _Device(
        id: 'ESP_05',
        name: 'Vườn 5 - Bơm không phản hồi',
        behavior: _PumpBehavior.noResponse),
  ];

  final Map<String, _Command> _commands = {};
  final Map<String, Map<String, dynamic>> _configs = {};
  final Map<String, List<Map<String, dynamic>>> _schedules = {};

  HttpServer? _server;
  int _port = 0;
  int _commandCounter = 0;
  int _scheduleCounter = 0;
  int _accessGeneration = 0;
  int _refreshGeneration = 0;

  int get port => _port;

  Future<void> start({int port = 0}) async {
    final address =
        allowLan ? InternetAddress.anyIPv4 : InternetAddress.loopbackIPv4;
    final server = await HttpServer.bind(address, port);
    _port = server.port;
    _server = server;
    server.listen(_handleRequest);
  }

  Future<void> stop() async {
    await _server?.close(force: true);
    _server = null;
  }

  /// Chạy một lệnh điều khiển và trả về nội dung cần in ra màn hình.
  Future<String> runCommand(String line) async {
    final parts = line.trim().split(RegExp(r'\s+'));
    switch (parts) {
      case ['status']:
        return _describeStatus();
      case ['offline', final number]:
        return _setDeviceOnline(number, isOnline: false);
      case ['online', final number]:
        return _setDeviceOnline(number, isOnline: true);
      case ['expire']:
        _accessGeneration++;
        return 'Access tokens rejected; the app should refresh them on its own.';
      case ['revoke']:
        _accessGeneration++;
        _refreshGeneration++;
        return 'All tokens rejected; the app should return to login.';
      case ['down']:
        await stop();
        return 'Server is down.';
      case ['up']:
        if (_server == null) await start(port: _port);
        return 'Server is up.';
      default:
        return _commandHelp;
    }
  }

  String _describeStatus() {
    final rows = _devices.indexed.map((entry) {
      final (index, device) = entry;
      final status = device.isOnline ? 'ONLINE' : 'OFFLINE';
      return '  ${index + 1}. ${device.id} $status';
    });
    return [
      ...rows,
      '  access generation $_accessGeneration, refresh generation $_refreshGeneration',
    ].join('\n');
  }

  String _setDeviceOnline(String number, {required bool isOnline}) {
    final index = int.tryParse(number);
    if (index == null || index < 1 || index > _devices.length) {
      return 'Device number must be 1..${_devices.length}.';
    }
    final device = _devices[index - 1];
    device.lastSeen = isOnline ? null : DateTime.now();
    return '${device.id} is ${isOnline ? 'ONLINE' : 'OFFLINE'}. Pull to refresh in the app.';
  }

  Future<void> _handleRequest(HttpRequest request) async {
    final reply = await _replyTo(request);
    if (logRequests && !_isPolling(request.uri.path)) {
      stdout
          .writeln('${request.method} ${request.uri.path} -> ${reply.status}');
    }
    try {
      request.response
        ..statusCode = reply.status
        ..headers.contentType = ContentType.json
        ..write(jsonEncode({
          'success': reply.status < 400,
          'statusCode': reply.status,
          'message': reply.message,
          if (reply.data != null) 'data': reply.data,
        }));
      await request.response.close();
    } on HttpException {
      // Lệnh `down` đã đóng kết nối này, không còn ai nhận phản hồi.
    }
  }

  bool _isPolling(String path) =>
      path.endsWith('/telemetry/latest') || path.endsWith('/ack');

  Future<_Reply> _replyTo(HttpRequest request) async {
    try {
      final body = await utf8.decoder.bind(request).join();
      final json = body.isEmpty ? const <String, dynamic>{} : jsonDecode(body);
      return _route(request, json as Map<String, dynamic>);
    } on FormatException catch (error) {
      return _Reply(400, 'Dữ liệu không hợp lệ: ${error.message}');
    } on TypeError {
      return const _Reply(400, 'Dữ liệu không hợp lệ.');
    }
  }

  _Reply _route(HttpRequest request, Map<String, dynamic> body) {
    final segments = request.uri.pathSegments;
    if (segments.length >= 3 && segments[0] == 'api' && segments[1] == 'auth') {
      return _routeAuth(request.method, segments[2], body);
    }
    if (segments.length < 2 ||
        segments[0] != 'api' ||
        segments[1] != 'devices') {
      return const _Reply(404, 'Không tìm thấy API.');
    }

    final account = _authenticate(request);
    if (account == null) {
      return const _Reply(401, 'Phiên đăng nhập không hợp lệ.');
    }
    if (account.role == 'ADMIN') {
      return const _Reply(403, 'Không có quyền truy cập.');
    }

    if (segments.length == 2) {
      return request.method == 'GET'
          ? _Reply(200, 'Success', _deviceRows(account))
          : const _Reply(405, 'Phương thức không được hỗ trợ.');
    }
    final device = _findDevice(segments[2], account);
    if (device == null) return const _Reply(404, 'Không tìm thấy thiết bị.');
    return _routeDevice(request, device, segments.sublist(3), body);
  }

  _Reply _routeAuth(String method, String action, Map<String, dynamic> body) {
    if (method != 'POST') {
      return const _Reply(405, 'Phương thức không được hỗ trợ.');
    }
    return switch (action) {
      'login' => _login(body),
      'register' => _register(body),
      'refresh-token' => _refreshSession(body),
      'logout' => const _Reply(200, 'Đăng xuất thành công'),
      _ => const _Reply(404, 'Không tìm thấy API.'),
    };
  }

  _Reply _routeDevice(
    HttpRequest request,
    _Device device,
    List<String> path,
    Map<String, dynamic> body,
  ) {
    switch ((request.method, path)) {
      case ('GET', ['telemetry']):
        return _readHistory(device, request.uri.queryParameters);
      case ('GET', ['telemetry', 'latest']):
        return _readLatest(device);
      case ('POST', ['control']):
        return _sendPumpCommand(device, body);
      case ('GET', ['commands', final commandId, 'ack']):
        return _readAcknowledgment(device, commandId);
      case ('GET', ['config']):
        return _Reply(200, 'Success', _configOf(device));
      case ('PUT' || 'POST', ['config']):
        return _updateConfig(device, body);
      case ('GET', ['schedules']):
        return _Reply(200, 'Success', _schedulesOf(device));
      case ('POST', ['schedules']):
        return _createSchedule(device, body);
      case ('PUT', ['schedules', final scheduleId]):
        return _updateSchedule(device, scheduleId, body);
      case ('DELETE', ['schedules', final scheduleId]):
        return _deleteSchedule(device, scheduleId);
      default:
        return const _Reply(404, 'Không tìm thấy API.');
    }
  }

  // --- Đăng nhập ---

  _Reply _login(Map<String, dynamic> body) {
    final account = _findAccount(body['email']);
    final password = _registeredPasswords[account?.email] ?? testPassword;
    if (account == null || body['password'] != password) {
      return const _Reply(401, 'Email hoặc mật khẩu không đúng.');
    }
    return _Reply(200, 'Login successfully', _sessionFor(account));
  }

  _Reply _register(Map<String, dynamic> body) {
    final email = body['email'];
    final password = body['password'];
    if (email is! String || !email.contains('@')) {
      return const _Reply(400, 'email: Không đúng định dạng email, ');
    }
    if (password is! String || password.length < 8) {
      return const _Reply(
          400, 'password: Mật khẩu phải có độ dài ít nhất là 8 kí tự, ');
    }
    if (_findAccount(email) != null) {
      return const _Reply(409, 'Email này đã được đăng ký trong hệ thống.');
    }
    final id = _accounts.length + _registeredAccounts.length + 1;
    _registeredAccounts[email] = _Account(id, email, 'USER', hasDevices: false);
    _registeredPasswords[email] = password;
    return const _Reply(201, 'Đăng ký tài khoản thành công!');
  }

  _Reply _refreshSession(Map<String, dynamic> body) {
    final claims = _decodeToken(body['refreshToken']);
    final isValid = claims != null &&
        claims['typ'] == 'refresh' &&
        claims['gen'] == _refreshGeneration &&
        !_isExpired(claims);
    final account = isValid ? _findAccount(claims['email']) : null;
    if (account == null) {
      return const _Reply(401, 'Refresh token không hợp lệ.');
    }
    return _Reply(200, 'Access Token is refreshed', _sessionFor(account));
  }

  Map<String, dynamic> _sessionFor(_Account account) => {
        'accessToken': _encodeToken(
            account, 'access', _accessGeneration, _accessTokenLifetime),
        'refreshToken': _encodeToken(
            account, 'refresh', _refreshGeneration, _refreshTokenLifetime),
        'role': account.role,
        'userResponse': {
          'id': account.id,
          'email': account.email,
          'phoneNumber': null,
          'role': account.role,
        },
      };

  _Account? _authenticate(HttpRequest request) {
    final header = request.headers.value(HttpHeaders.authorizationHeader);
    if (header == null || !header.startsWith('Bearer ')) return null;
    final claims = _decodeToken(header.substring('Bearer '.length));
    if (claims == null ||
        claims['typ'] != 'access' ||
        claims['gen'] != _accessGeneration ||
        _isExpired(claims)) {
      return null;
    }
    return _findAccount(claims['email']);
  }

  _Account? _findAccount(Object? email) {
    for (final account in [..._accounts, ..._registeredAccounts.values]) {
      if (account.email == email) return account;
    }
    return null;
  }

  String _encodeToken(
      _Account account, String type, int generation, Duration lifetime) {
    final expiresAt =
        DateTime.now().add(lifetime).millisecondsSinceEpoch ~/ 1000;
    return [
      _encodeSegment({'alg': 'none'}),
      _encodeSegment({
        'typ': type,
        'gen': generation,
        'userId': account.id,
        'email': account.email,
        'exp': expiresAt,
      }),
      'fake',
    ].join('.');
  }

  String _encodeSegment(Map<String, dynamic> json) =>
      base64Url.encode(utf8.encode(jsonEncode(json))).replaceAll('=', '');

  Map<String, dynamic>? _decodeToken(Object? token) {
    if (token is! String) return null;
    final parts = token.split('.');
    if (parts.length != 3) return null;
    try {
      final payload =
          utf8.decode(base64Url.decode(base64Url.normalize(parts[1])));
      return jsonDecode(payload) as Map<String, dynamic>;
    } on FormatException {
      return null;
    }
  }

  bool _isExpired(Map<String, dynamic> claims) {
    final now = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    return (claims['exp'] as int) <= now;
  }

  // --- Thiết bị ---

  List<Map<String, dynamic>> _deviceRows(_Account account) {
    if (!account.hasDevices) return const [];
    return _devices
        .map((device) => {
              'id': device.id,
              'device_name': device.name,
              'status': device.isOnline ? 'active' : 'offline',
              'permission': 'owner',
            })
        .toList();
  }

  _Device? _findDevice(String id, _Account account) {
    if (!account.hasDevices) return null;
    for (final device in _devices) {
      if (device.id == id) return device;
    }
    return null;
  }

  // --- Cảm biến ---

  _Reply _readLatest(_Device device) {
    if (!device.hasData) return const _Reply(200, 'Success');
    return _Reply(
        200, 'Success', _readingAt(device, device.lastSeen ?? DateTime.now()));
  }

  _Reply _readHistory(_Device device, Map<String, String> query) {
    final fromText = query['from'];
    final toText = query['to'];
    if (fromText == null || toText == null) {
      return const _Reply(400, 'Thiếu khoảng thời gian truy vấn.');
    }
    final from = DateTime.parse(fromText);
    var to = DateTime.parse(toText);
    if (from.isAfter(to)) {
      return const _Reply(400, 'Khoảng thời gian truy vấn không hợp lệ');
    }
    if (!device.hasData) return const _Reply(200, 'Success', <Object>[]);

    final lastSeen = device.lastSeen;
    if (lastSeen != null && lastSeen.isBefore(to)) to = lastSeen;

    final stepMs = _historyStep.inMilliseconds;
    final firstStep = (from.millisecondsSinceEpoch / stepMs).ceil();
    final lastStep = to.millisecondsSinceEpoch ~/ stepMs;
    final rows = [
      for (var step = firstStep; step <= lastStep; step++)
        _readingAt(device, DateTime.fromMillisecondsSinceEpoch(step * stepMs)),
    ];
    return _Reply(200, 'Success', rows);
  }

  /// Giá trị chỉ phụ thuộc vào thiết bị và thời điểm, nên lịch sử không đổi giữa các lần gọi.
  Map<String, dynamic> _readingAt(_Device device, DateTime time) {
    final step = time.millisecondsSinceEpoch ~/ _telemetryStep.inMilliseconds;
    final recordedAt = DateTime.fromMillisecondsSinceEpoch(
        step * _telemetryStep.inMilliseconds);
    final hours =
        recordedAt.millisecondsSinceEpoch / Duration.millisecondsPerHour;
    final offset = _devices.indexOf(device).toDouble();
    return {
      'id': step,
      'device_id': device.id,
      'soil_moisture': _rounded(55 + 20 * sin(hours / 3 + offset)),
      'temperature': _rounded(28 + 4 * sin(hours / 6 + offset)),
      'humidity': _rounded(65 + 12 * cos(hours / 5 + offset)),
      'water_level': _rounded(70 + 15 * cos(hours / 8 + offset)),
      'recorded_at': recordedAt.toIso8601String(),
    };
  }

  double _rounded(double value) => (value * 10).roundToDouble() / 10;

  // --- Máy bơm ---

  _Reply _sendPumpCommand(_Device device, Map<String, dynamic> body) {
    final action = body['action']?.toString().toUpperCase();
    if (action != 'ON' && action != 'OFF') {
      return const _Reply(400, 'action chỉ nhận giá trị ON hoặc OFF');
    }
    if (body['duration_seconds'] != null && action != 'ON') {
      return const _Reply(400, 'duration_seconds chỉ hợp lệ khi bật bơm');
    }
    final commandId = 'cmd-${++_commandCounter}';
    _commands[commandId] = _Command(device, DateTime.now());
    return _Reply(200, 'Đã gửi lệnh, đang chờ thiết bị xác nhận', {
      'device_id': device.id,
      'action': action,
      'status': 'PENDING',
      'command_id': commandId,
    });
  }

  _Reply _readAcknowledgment(_Device device, String commandId) {
    final command = _commands[commandId];
    if (command == null || command.device != device) {
      return const _Reply(404, 'Không tìm thấy trạng thái xác nhận lệnh');
    }
    final (status, message) = _acknowledgmentOf(command);
    return _Reply(200, 'Success', {
      'command_id': commandId,
      'device_id': device.id,
      'status': status,
      if (message != null) 'message': message,
      'timestamp': DateTime.now().toUtc().toIso8601String(),
    });
  }

  (String, String?) _acknowledgmentOf(_Command command) {
    final isWaiting =
        DateTime.now().difference(command.createdAt) < _pumpAckDelay;
    if (!command.device.isOnline || isWaiting) return ('PENDING', null);
    return switch (command.device.behavior) {
      _PumpBehavior.succeeds => ('SUCCEEDED', null),
      _PumpBehavior.rejects => ('FAILED', 'Thiết bị từ chối lệnh bơm.'),
      _PumpBehavior.noResponse => ('PENDING', null),
    };
  }

  // --- Tự động hóa ---

  Map<String, dynamic> _configOf(_Device device) => _configs.putIfAbsent(
        device.id,
        () => {
          'device_id': device.id,
          'auto_mode': false,
          'soil_moisture_min': 40.0,
          'soil_moisture_max': 80.0,
          'max_watering_duration': 2,
          'air_humidity_min': 50.0,
          'air_humidity_max': 80.0,
          'air_temperature_min': 20.0,
          'air_temperature_max': 35.0,
          'water_level_min': 20,
        },
      );

  _Reply _updateConfig(_Device device, Map<String, dynamic> body) {
    final config = _configOf(device)..addAll(body);
    return _Reply(200, 'Đã cập nhật cấu hình, đang chờ thiết bị xác nhận', {
      ...config,
      'command_id': 'cmd-${++_commandCounter}',
      'command_status': 'PENDING',
    });
  }

  List<Map<String, dynamic>> _schedulesOf(_Device device) =>
      _schedules.putIfAbsent(device.id, () => []);

  _Reply _createSchedule(_Device device, Map<String, dynamic> body) {
    final schedule = {
      ...body,
      'id': ++_scheduleCounter,
      'device_id': device.id
    };
    _schedulesOf(device).add(schedule);
    return _Reply(201, 'Tạo lịch tưới thành công', schedule);
  }

  _Reply _updateSchedule(
      _Device device, String scheduleId, Map<String, dynamic> body) {
    final schedule = _findSchedule(device, scheduleId);
    if (schedule == null) return const _Reply(404, 'Không tìm thấy lịch tưới');
    schedule.addAll(body);
    return _Reply(200, 'Cập nhật lịch tưới thành công', schedule);
  }

  _Reply _deleteSchedule(_Device device, String scheduleId) {
    final schedule = _findSchedule(device, scheduleId);
    if (schedule == null) return const _Reply(404, 'Không tìm thấy lịch tưới');
    _schedulesOf(device).remove(schedule);
    return const _Reply(200, 'Xóa lịch tưới thành công');
  }

  Map<String, dynamic>? _findSchedule(_Device device, String scheduleId) {
    final id = int.tryParse(scheduleId);
    for (final schedule in _schedulesOf(device)) {
      if (schedule['id'] == id) return schedule;
    }
    return null;
  }
}
