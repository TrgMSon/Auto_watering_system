# Firmware Integration Notes — F1.2a

Ngày đối chiếu: 04/11/2026. Nền source: `origin/main` tại
`f3084ac148246b3aad5d237d329a471040f67b22`.

Đây là ghi nhận hợp đồng và source, không phải bằng chứng firmware đã được
triển khai. `../esp32.ino` vẫn rỗng. Chưa điều khiển GPIO/relay, kết nối
BLE/MQTT hoặc lưu credential. Không sửa Backend/Mobile để giải quyết khác biệt.

## 1. Nguồn hợp đồng và định danh

- [Hợp đồng BLE gốc](hop-dong-BLE-chot-2026-11-04.md) được sao chép nguyên
  nội dung từ file người phụ trách cung cấp, không chỉnh sửa các quyết định.
- Device ID chính thức và MAC Wi-Fi STA: `38:3E:51:6F:C7:A8`.
- Dùng MAC sáu cặp hex viết hoa, có dấu `:` trong Backend, MQTT topic và
  `device_id` khi schema có trường này. Không dùng địa chỉ BLE ngẫu nhiên.
- Advertising name: `WS-383E516FC7A8`; ưu tiên tên trong scan response,
  service UUID trong advertising. Không bảo đảm mọi điện thoại hiển thị đủ tên.
- Backend `CreateDeviceRequest.java:11–15` kiểm tra sáu cặp hex;
  `DeviceServiceImpl.java:100` chuẩn hóa `-` thành `:` và chữ hoa.

Các đường dẫn source dưới đây tính từ repository root, dòng tại commit nền.

## 2. Quy ước BLE đã chốt

Mọi UUID dùng prefix `07975e87-679e-531b-b48b-d227f457` nối với hậu tố sau;
code tương lai phải dùng chuỗi UUID đầy đủ.

| Hậu tố | Vai trò | Quyền |
|---|---|---|
| `0000` | Service chung | Service |
| `0001` | Sensor/trạng thái bơm | Read, Notify |
| `0002` | Pump command | Write With Response |
| `0003` | Device config | Write With Response |
| `0004` | Nonce | Read |
| `0005` | Kết quả nghiệp vụ | Notify |
| `0006` | Wi-Fi provisioning | Write With Response, yêu cầu bảo mật BLE |
| `0007` | Unix time UTC | Write With Response |

- `0001`: `{"soil":42.5,"temp":31.8,"hum":68.0,"water":25.4,"pump":0}`.
  Đơn vị lần lượt %, °C, %, cm; pump 0/1; cảm biến lỗi là `null`.
  Notify mục tiêu 2 giây.
- `0002`: `{"cmd":"PUMP_ON","dur":300,"sig":"..."}` hoặc
  `{"cmd":"PUMP_OFF","dur":0,"sig":"..."}`. ON yêu cầu `1 <= dur <= max_dur`
  đang lưu, đủ nước và điều kiện an toàn; OFF không bị chặn bởi PUMP_BUSY.
- `0003`: gửi đủ `min`, `max`, `auto`, `max_dur`, `min_water`, `sig`.
  `0 <= min < max <= 100`, auto 0/1; `max_dur` 60–600 giây, mặc định 120;
  `min_water` là cm. Chỉ lưu cấu hình sau xác thực và kiểm tra hợp lệ.
- `0006`: `{"ssid":"HomeWiFi","pass":"12345678","sig":"..."}` là ví dụ
  trong hợp đồng, không phải credential thật. Pairing/bonding và mã hóa là
  bắt buộc, ESP32 phải kiểm tra trạng thái bảo mật. HMAC không mã hóa password.
  Kết nối tối đa 30 giây; chỉ lưu NVS khi thành công; thất bại giữ cấu hình cũ.
  Notify `{"op":"WIFI","code":"WIFI_OK"}` hoặc WIFI_FAIL qua `0005`.
- `0007`: `{"unix":1793800000,"sig":"..."}`, Unix UTC giây; không có RTC
  rời có pin được xác nhận. Chỉ chấp nhận thời gian sau xác thực.
- `0005`: ví dụ `{"op":"PUMP_ON","code":"OK"}`. GATT response chỉ xác nhận
  nhận write; không thay kết quả nghiệp vụ. Bật Notify trước khi gửi lệnh.
- Một lệnh chờ mỗi lúc; timeout thông thường 3 giây, riêng Wi-Fi tối đa
  30 giây cộng biên truyền Notify. Không tự lặp ON khi chưa rõ trạng thái.
- Tối đa một Mobile kết nối; advertising lại khi disconnect, giữ logic auto.
- MTU 185 là mục tiêu, không bảo đảm; 182 byte chỉ khi đàm phán thành công.
  Chưa có hợp đồng phân mảnh/long write.

12 mã cố định: `OK`, `BAD_SIG`, `NONCE_EXPIRED`, `BAD_JSON`, `OUT_OF_RANGE`,
`LOW_WATER`, `MAINTENANCE`, `SENSOR_ERROR`, `PUMP_BUSY`, `INVALID_TIME`,
`WIFI_OK`, `WIFI_FAIL`. Không dùng BUSY hoặc UNAUTHORIZED.

### HMAC và nonce

Key là UTF-8 của nguyên chuỗi `ble_secret_key`, **không decode Base64URL**.
Message UTF-8; HMAC-SHA256 lấy 16 ký tự hex thường đầu tiên.

| Lệnh | Chuỗi ký |
|---|---|
| Pump | `nonce|cmd|dur` |
| Config | `nonce|CONFIG|min|max|auto|max_dur|min_water` |
| Wi-Fi | `nonce|WIFI|ssid|pass` |
| Time | `nonce|TIME|unix` |

Số thực min/max/min_water đúng một chữ số thập phân; số nguyên dùng thập phân,
không khoảng trắng. Nonce 8 byte ngẫu nhiên bảo mật, 16 hex thường, hết hạn
30 giây, dùng một lần. READ `0004` trước từng WRITE `0002/0003/0006/0007`;
tiêu thụ nonce cả khi chữ ký/lệnh không hợp lệ.

Ba vector gốc đã PASS trên ESP32 theo báo cáo người phụ trách; CONFIG cũ ba
trường chỉ là lịch sử. F1.1 tính lại bằng .NET khớp ba vector gốc, CONFIG mở
rộng và TIME. Chưa có kiểm thử ESP32 cho CONFIG mở rộng/TIME/WIFI, nonce,
pairing hoặc giao tiếp BLE thực tế. Vector và khóa thử xem hợp đồng gốc;
không dùng khóa thử cho thiết bị thật.

## 3. MQTT thực tế từ Backend

Tiền tố: `iot/garden/{deviceId}`, ví dụ
`iot/garden/38:3E:51:6F:C7:A8`. Mobile dùng REST, không kết nối broker trực tiếp.

| Hướng | Topic | Trường JSON |
|---|---|---|
| ESP32 → Backend | `/telemetry` | `deviceId`, `soilMoisture`, `temperature`, `humidity`, `waterLevel`, `recordedAt` |
| ESP32 → Backend | `/status` | `status`, `firmware_version` |
| ESP32 → Backend | `/ack` | `device_id`, `command_id`, `status`, `action`, `message` |
| Backend → ESP32 | `/cmd` | `command_id`, `device_id`, `timestamp`, `action`, `params` |
| Backend → ESP32 | `/config` | `command_id`, `device_id`, `timestamp`, `config` |

### Command/config

`backend/src/main/java/com/project/backend/service/impl/MqttDeviceCommandPublisher.java:91–126`:

- Command: `action="PUMP_CONTROL"`; params gồm `state="ON"|"OFF"`, tùy chọn
  `duration_seconds` (số nguyên >= 1). Không đồng nhất với REST action ON/OFF.
- Config không có action trong envelope; object `config` chứa các trường dưới.
- `command_id="CMD-" + UUID`, timestamp là `Instant.now().toString()`.
- Retry publish tối đa 3 lần với cùng payload; firmware cần tránh thực thi
  lại lệnh trùng. Hành vi khi ON thiếu duration chưa được chốt cho firmware.

`backend/src/main/java/com/project/backend/service/impl/DeviceServiceImpl.java:507–518`:

| Config MQTT | Ý nghĩa/đối chiếu |
|---|---|
| `auto_mode` | Boolean |
| `soil_moisture_min`, `soil_moisture_max` | Ngưỡng soil |
| `max_watering_duration_minutes` | Tên trường biểu đạt phút; còn mâu thuẫn Mobile |
| `air_humidity_min`, `air_humidity_max` | Ngưỡng độ ẩm không khí |
| `air_temperature_min`, `air_temperature_max` | Ngưỡng nhiệt độ |
| `min_water_level_alert` | Nhận giá trị waterLevelMin; cần xác nhận cm với Backend |

### Telemetry/status/ACK

- `backend/src/main/java/com/project/backend/dto/sensor/SensorLogRequest.java:8–14`
  dùng camelCase, số cảm biến Float, recordedAt LocalDateTime. Không áp dụng
  schema BLE hoặc snake_case của REST response cho telemetry MQTT.
- `.../service/impl/SensorDataServiceImpl.java:45–73` lấy ID từ topic; không
  dùng deviceId payload để chọn thiết bị. Thiếu recordedAt thì dùng giờ server.
  DTO không có trạng thái bơm. Đơn vị nước, timezone và dữ liệu lỗi cần chốt.
- `.../dto/device/DeviceStatusMessage.java:5` dùng `firmware_version`.
  `.../service/impl/DeviceLifecycleServiceImpl.java:49–137` xử lý ACTIVE,
  OFFLINE, MAINTENANCE; heartbeat từ ACTIVE/telemetry, hết hạn 30 giây,
  scheduler mỗi 10 giây. Tài liệu yêu cầu ACTIVE khi kết nối, OFFLINE trong LWT.
- `.../service/MqttCommandAcknowledgmentService.java:74–95`: device_id phải
  khớp topic; command_id/status phải có. Nhận SUCCESS/SUCCEEDED hoặc FAILED/ERROR.
  Nhánh thành công dùng action ON/OFF để ghi lịch sử; gọi action.equals nên
  thiếu action có thể lỗi null. Schema ACK config và thông báo tự tắt chưa chốt.
  Không tự bịa action config để né lỗi Backend.
- Hàm updateStatus bỏ qua ACK trùng/không biết nhưng caller vẫn có thể chạy
  tác dụng phụ ghi lịch sử; cần Backend xác nhận xử lý idempotency.
- `.../config/MqttConfig.java:89–99,130–138`: subscribe telemetry QoS 0,
  status/ack QoS 1; publish QoS 1, đồng bộ, completion timeout 5 giây.
  Clean session true; chưa có bằng chứng giao lệnh offline bền vững/retain.

## 4. Secret, credential và bảo mật

- `backend/.../service/impl/DeviceServiceImpl.java:99–133,171–174`: BLE secret
  và MQTT password được sinh riêng từ 32 byte ngẫu nhiên, Base64URL không
  padding (43 ký tự). Username của board này là `dev_383E516FC7A8`.
- Response tạo thiết bị trả `ble_secret_key`, `mqtt_username`, `mqtt_password`;
  Backend tạo cùng user trên EMQX. Liệt kê thiết bị không trả credential.
- `backend/.../controller/MobileDeviceController.java:39–44` có API
  GET `/api/devices/{id}/ble-secret-key`, trả nguyên secret qua lớp service.
  Mobile hiện chưa gọi endpoint này; model ble_pass_key không thay thế API.
- Chưa có đường cấp secret/MQTT credential ban đầu cho ESP32. BLE 0006 chỉ có
  Wi-Fi SSID/password; không tự thêm credential MQTT vào payload này.
- `backend/.../config/MqttConfig.java:64–79` dùng user/password và SSL socket
  factory mặc định khi URL bắt đầu ssl://; source không bắt buộc TLS cho mọi URL.
- `backend/.../service/EmqxManagementService.java:43–53` dùng Basic Auth cho
  Management API; README yêu cầu HTTPS, source kiểm tra cấu hình không rỗng
  nhưng chưa ép scheme HTTPS. EMQX Management credential không đưa vào ESP32.
- Cần xác nhận broker host/port, CA/trust, kiểm tra hostname, thời gian cho TLS,
  client ID và ACL giới hạn topic theo MAC. Chưa xác minh deployment thực tế.
- Không ghi password/secret ra log hoặc repository; NVS không mặc nhiên có
  mã hóa chỉ vì dùng NVS. Cơ chế bảo vệ credential lưu trữ cần xác nhận riêng.

## 5. Alignment và khác biệt cần xử lý

| Hạng mục | Trạng thái | Khác biệt/việc cần thống nhất |
|---|---|---|
| MAC có dấu hai chấm | Đã chốt, Backend có implementation | Firmware/Mobile BLE chưa triển khai |
| BLE UUID, payload, nonce, 12 mã | Đã chốt, chưa triển khai | Chưa có GATT client/server thực tế trong source |
| HMAC | Đã chốt, test thuật toán một phần | Chưa kiểm thử giao thức mở rộng trên ESP32 |
| Phút/giây | Mâu thuẫn source và hợp đồng | Mobile hiển thị giây, mặc định 120, slider 10–300, gửi nguyên giá trị; Backend publish trường phút |
| Giới hạn cấu hình | Chưa đồng nhất | Backend chỉ Min(1), chưa Max(10); cho phép soil min=max trong khi BLE yêu cầu min<max |
| Mực nước | Chốt BLE cm; MQTT còn thiếu xác nhận | Backend Integer, BLE một số thập phân; chưa chốt chuyển đổi/làm tròn |
| Trạng thái bơm | Mobile có suy ra cục bộ | Timer sau ACK không phản ánh đầy đủ BLE/auto/reboot; MQTT telemetry thiếu pump |
| Config ACK/đồng bộ | Thiếu hợp đồng | Mobile PUT config chưa đợi ACK; chưa có quy tắc khôi phục khi ESP32 reboot hoặc BLE đổi cấu hình |
| Wi-Fi bảo mật | Đã yêu cầu, chưa đủ chi tiết | Chốt pairing/MITM/bond reset, escape ký tự pipe, MTU/long write; chưa nhận password thật |
| Đọc Device ID qua BLE | Thiếu schema | Tài liệu yêu cầu fallback đọc ID nhưng bảng GATT chưa chỉ rõ vị trí/payload |
| Lịch tưới | Backend/Mobile có CRUD | Chưa thấy publisher lịch hoặc scheduler thực thi; chưa chốt phía chạy lịch/timezone |
| OTA | Chưa triển khai/chưa có hợp đồng | Metadata firmware_version không phải OTA |

Bằng chứng mâu thuẫn thời gian:
`mobile/lib/features/automation/presentation/widgets/advanced_config_bottom_sheet.dart:50,152–157`;
`mobile/lib/features/automation/data/models/device_config_model.dart:39,53`;
`backend/src/main/java/com/project/backend/dto/device/DeviceConfigRequest.java:29–32`;
`backend/src/main/java/com/project/backend/service/impl/DeviceServiceImpl.java:470–473,512`.
Không sửa Backend/Mobile và không đoán đơn vị trong firmware để bù khác biệt.

## 6. Cấu trúc firmware đề xuất và điều kiện build

Chưa tạo skeleton C++ ở F1.2a. Giữ `firmware/esp32.ino` làm entry point theo yêu cầu.

```text
firmware/
  esp32.ino             # setup/loop điều phối sau khi chốt build
  board_config.h        # GPIO/polarity/calibration, không chứa secret
  sensors.h/.cpp        # soil, DHT11, SRF05
  pump.h/.cpp           # relay, timer, điều kiện dừng
  connectivity.h/.cpp   # Wi-Fi/MQTT, reconnect, LWT
  protocol.h/.cpp       # schema, chuyển đơn vị, command dedup/ACK
  ble_service.h/.cpp    # GATT, bảo mật, HMAC/nonce
  storage.h/.cpp        # NVS, chỉ lưu dữ liệu đã được chấp nhận
  docs/
```

Ghi nhận tại F1.2a (đã được bổ sung bằng khảo sát F1.2b-1 ở mục 9):

1. Chưa xác nhận model board/FQBN, phiên bản Arduino IDE và Arduino ESP32 core.
2. Chưa tìm thấy arduino-cli/arduino/arduino-builder trong PATH. Có thư mục
   Arduino15 nhưng điều đó chưa chứng minh có toolchain phù hợp hoặc build được.
3. Arduino sketch cần thống nhất tên thư mục và primary .ino; thư mục firmware
   và entry esp32.ino hiện khác tên. Cần chốt cách staging/mở sketch mà vẫn giữ
   entry path này; chưa đổi tên, thêm primary sketch khác hoặc di chuyển file.

Không cài toolchain/dependency và không tuyên bố đã biên dịch/chạy trên ESP32.

## 7. Quyết định trước F2

- Board/core đã xác định ở mục 9; cần xác nhận các tùy chọn board và kiểm chứng
  quy trình staging/build, giữ entry point đã yêu cầu.
- Cung cấp GPIO soil/DHT11/SRF05 TRIG/ECHO/relay, relay active HIGH/LOW,
  sơ đồ điện/nguồn, hiệu chuẩn soil và kích thước/vị trí cảm biến bồn.
- Backend/Mobile thống nhất phút/giây, giới hạn 1–10 phút, default 2 phút;
  firmware dùng giây và kiểm thử chuyển đổi, không tự sửa giao thức.
- Chốt cấp secret/MQTT credential, TLS/ACL và bảo mật BLE/Wi-Fi còn thiếu.
- Chốt ACK config/tự tắt, ON không duration, ưu tiên BLE/MQTT/auto,
  mực nước MQTT, đồng bộ cấu hình và Device ID qua BLE.
- Khi được duyệt: chuẩn bị skeleton/build trước; F2 đọc cảm biến và điều khiển
  bơm có timer tại chỗ trước khi tích hợp mạng. Chưa mở rộng lịch/OTA nếu chưa chốt.

## 8. Kiểm chứng F1.2a

- Nhánh firmware/esp32 tạo từ origin/main f3084ac; không merge/reset main.
- Chỉ thêm hai tài liệu dưới firmware/docs; không sửa Backend/Mobile.
- Bản hợp đồng sao chép được kiểm tra SHA-256 trùng file gốc.
- Kiểm tra diff/whitespace và working tree; không build do chưa đủ điều kiện
  nêu trên. Không kiểm thử phần cứng, không commit/push.

## 9. F1.2b-1 — Môi trường build và chuẩn bị phần cứng

### 9.1. Trạng thái repository và phạm vi

- Branch `firmware/esp32`, HEAD `f3084ac148246b3aad5d237d329a471040f67b22`.
- Giữ nguyên thay đổi F1.2a: ngoại lệ .gitignore và hai tài liệu firmware/docs.
  Bước này chỉ cập nhật tài liệu ghi chú tích hợp; không sửa hợp đồng BLE,
  Backend, Mobile, frontend hoặc esp32.ino. Không commit/push.
- Board, FQBN, port và kết quả chạy HMAC/MAC bên dưới do người dùng xác nhận;
  không suy ra từ việc dò cổng USB. Không mở COM10, upload hoặc chạy relay.

### 9.2. Công cụ và phiên bản đã tìm thấy

| Thành phần | Kết quả | Bằng chứng local |
|---|---|---|
| Arduino IDE | 2.3.10 | `C:/Users/nguye/AppData/Local/Programs/Arduino IDE/resources/app/package.json` |
| Arduino CLI đi kèm IDE | 1.5.1, commit 01f3d4f2b | Chạy lệnh version của `resources/app/lib/backend/resources/arduino-cli.exe` |
| CLI trong PATH | Không tìm thấy | Get-Command arduino-cli/arduino/arduino-builder |
| PlatformIO | Chưa tìm thấy trong PATH hoặc thư mục .platformio của user | Không kết luận không tồn tại ở mọi vị trí khác |
| Arduino ESP32 Core | 3.3.12 | `C:/Users/nguye/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.12/platform.txt`: version=3.3.12 |
| Board | ESP32 Dev Module | Người dùng xác nhận; boards.txt của core có esp32.name tương ứng |
| FQBN | esp32:esp32:esp32 | Người dùng xác nhận; target/mcu esp32 trong boards.txt |
| Framework / port | Arduino / COM10 | Người dùng xác nhận; port chỉ cần khi upload, không cần cho compile |
| Sketchbook | `E:/PTIT/Ki1Nam4/IOT/Arduino` | `C:/Users/nguye/.arduinoIDE/arduino-cli.yaml`, directories.user |

Không chọn/nâng cấp core hoặc framework. Chỉ xác nhận phiên bản đang cài,
chưa xác nhận mọi chương trình thử trước đây được build bằng chính core này.
Người dùng xác nhận ESP32 đã nạp/chạy HMAC-SHA256 và đọc MAC Wi-Fi thành công;
không coi đó là kiểm thử firmware repository hiện tại.

### 9.3. Thư viện hiện có và phần chưa chọn

| Chức năng | Hiện trạng | Quyết định tiếp theo |
|---|---|---|
| DHT11 | DHT sensor library 1.4.7; Adafruit Unified Sensor 1.1.15 trong sketchbook/libraries | Có thể dùng bản đã cài sau khi kiểm chứng compile; dependency khai báo trong library.properties |
| BLE | Core 3.3.12 có thư mục libraries/BLE | Chưa chọn BLE API/stack cuối cùng; kiểm chứng pairing/bonding, bảo mật GATT, MTU trước production; không tự cài NimBLE |
| Wi-Fi / TLS | Core có WiFi, NetworkClientSecure | Chưa kiểm thử kết nối, CA/host/port/ACL vẫn cần chốt |
| NVS | Core có Preferences | Chưa lưu credential hoặc cấu hình |
| HMAC | Người dùng đã kiểm thử mbedTLS trên ESP32 | Giữ hướng đã kiểm thử; cần compile lại với core hiện có khi triển khai |
| MQTT | Chưa thấy thư viện MQTT trong sketchbook/libraries hoặc danh sách thư viện core đã kiểm tra | Chưa chọn thư viện/phiên bản; cần đáp ứng QoS/ACK, kích thước JSON và reconnect đã chốt; không cài dependency ở bước này |
| Soil moisture | Chưa xác nhận loại module hoặc dùng AO/DO | Chưa chọn driver; nếu dùng analog cần ADC và hiệu chuẩn; không tự gán GPIO |
| SRF05 | Chưa thấy thư viện SRF05 riêng trong thư mục thư viện đã kiểm tra | Chưa quyết định driver/thư viện; cần timeout đo và kiểm chứng mức điện áp ECHO |
| JSON | Chưa chọn dependency cho protocol | Chốt thư viện/phiên bản khi chuẩn bị parser, không tự cài |

Phạm vi dò thư viện: core 3.3.12/libraries, Arduino15/libraries và
`E:/PTIT/Ki1Nam4/IOT/Arduino/libraries`. Không quét nội dung sketch cá nhân để
suy diễn GPIO hoặc coi chương trình thử là sơ đồ phần cứng đã được duyệt.

### 9.4. Cách build giữ entry point firmware/esp32.ino

Không mở trực tiếp thư mục firmware rồi mặc định IDE nhận đây là sketch hợp lệ:
primary .ino và thư mục sketch cần cùng tên. Đề xuất quy trình staging sau,
không đổi tên hoặc di chuyển entry point trong repository:

1. Khi có skeleton được duyệt, tạo thư mục tạm có tên `esp32` ngoài source
   repository; sao chép esp32.ino và các file .h/.cpp cần build cùng cấu trúc.
   Chỉ dùng bản sao để Verify; chỉnh sửa nguồn tại firmware rồi sao chép lại.
2. Arduino IDE: mở bản sao `esp32/esp32.ino`; chọn ESP32 Dev Module với
   ESP32 core 3.3.12. Xác nhận Flash Size, Partition Scheme, PSRAM và các menu
   board theo module thực tế; hiện chưa có bằng chứng để chốt các tùy chọn này.
3. Chỉ dùng Verify, không Upload. COM10 không được mở trong bước chuẩn bị.
4. Có thể dùng CLI đi kèm IDE với cùng file cấu hình. Mẫu lệnh PowerShell dưới
   đây chỉ để tài liệu, CHƯA chạy; đường dẫn staging/build phải được tạo trước:

```powershell
& 'C:\Users\nguye\AppData\Local\Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' compile `
  --config-file 'C:\Users\nguye\.arduinoIDE\arduino-cli.yaml' `
  --fqbn esp32:esp32:esp32 `
  --build-path '<thu-muc-build-tam>' `
  '<thu-muc-staging>\esp32'
```

Không có --upload hoặc --port. Chưa tạo staging/script/skeleton trong bước
này. esp32.ino hiện 0 byte, chưa có setup/loop nên chưa có chương trình hoàn
chỉnh để Verify. Việc tìm thấy core/CLI chưa chứng minh build thành công.

### 9.5. Bảng GPIO và nguồn cần người dùng xác nhận

Không có số GPIO nào được chốt ở bước này. Điền cả số GPIO trên chip và nhãn
chân trên board để tránh nhầm thứ tự chân vật lý.

| Thiết bị / tín hiệu | GPIO đã duyệt | Nguồn/GND cần xác nhận | Thông tin còn thiếu |
|---|---|---|---|
| Soil moisture AO hoặc DO | Chưa xác nhận | VCC=?, GND=? | Model module, analog/digital, điện áp output, hiệu chuẩn khô/ướt; nếu AO cần kiểm tra ADC khi Wi-Fi hoạt động |
| DHT11 DATA | Chưa xác nhận | VCC=?, GND=? | Module hay sensor rời, điện trở pull-up và điện áp kéo DATA lên |
| SRF05 TRIG | Chưa xác nhận | VCC=?, GND=? | Model/pinout module, mức input TRIG chấp nhận |
| SRF05 ECHO | Chưa xác nhận | Chung GND theo sơ đồ được duyệt | Điện áp ECHO, có chia áp/level shifter chưa; cấu hình chân MODE nếu module có |
| Relay IN | Chưa xác nhận | VCC/JD-VCC/GND theo module | Active HIGH/LOW, 3.3 V input compatibility, jumper/opto isolation, trạng thái input lúc boot |
| Tiếp điểm relay / bơm | Không nối GPIO trực tiếp | Nguồn bơm riêng theo định mức | COM/NO/NC, điện áp/dòng khởi động, định mức tiếp điểm và bảo vệ tải cảm |
| ESP32 power | Không áp dụng | USB/5V/VIN/3V3 và GND theo board | Model/sơ đồ board, cách cấp nguồn, tránh đấu chồng hai nguồn không được hỗ trợ |
| GND toàn hệ thống | Không áp dụng | Chưa xác nhận sơ đồ nối mass | Common ground phía logic khi cần; nếu cách ly quang phải theo đúng thiết kế module |

ESP32 Dev Module là lựa chọn build, chưa xác định chính xác module PCB,
flash/PSRAM, pinout và sơ đồ nguồn của board đang cầm.

### 9.6. Kiểm tra an toàn phần cứng trước khi cho phép chạy bơm

- Relay phải OFF từ lúc cấp nguồn, reset và trước setup; code chạy sau boot
  không đủ bảo đảm trạng thái trước boot. Xác nhận polarity, mức input mặc định
  và phần cứng bias phù hợp; không tự chọn HIGH/LOW hoặc pull-up/pull-down.
- Kiểm tra ban đầu khi ngắt tải bơm; chưa kích relay ở F1.2b-1. Xác nhận GPIO
  không xung đột boot/flash, có khả năng output và phù hợp board cụ thể.
- Đặt timer cục bộ cho mọi đường bật bơm. Hợp đồng cấu hình max_dur=60–600 giây,
  mặc định 120 giây; thử phần cứng có giám sát với giới hạn ngắn đã được duyệt.
  Chưa chạy bơm tự động trong bước chuẩn bị.
- Không bật bơm khi nước thấp hoặc phép đo nước lỗi/timeout; khi chạy phải có
  kiểm tra để dừng nếu điều kiện này xuất hiện. Không đổi lỗi cảm biến thành 0.
- GPIO ESP32 dùng mức logic 3.3 V; không nối tín hiệu 5 V trực tiếp. Kiểm tra
  ECHO SRF05, AO soil, pull-up DHT11 và relay input trước khi nối; nếu vượt
  giới hạn phải có mạch chuyển mức/chia áp được xác nhận. Không lấy giới hạn
  tuyệt đối làm mức vận hành bình thường.
- Không cấp bơm/cuộn relay trực tiếp từ GPIO. Xác nhận nguồn, dòng khởi động,
  driver, chống nhiễu/bảo vệ tải cảm phù hợp; tránh sụt nguồn làm ESP32 reset.
- SRF05 đo khoảng cách cần đổi sang chiều cao nước bằng hình học bồn và vị trí
  gắn đã xác nhận; chưa có số liệu thì chưa thể kết luận nước đủ để tưới.

Nguồn kiểm tra điện áp và hạn chế chân:
[Espressif ESP32 Series Datasheet](https://documentation.espressif.com/esp32_datasheet_en.html),
các mục IO pins, Boot Configurations và Electrical Characteristics. Datasheet
module cảm biến/relay cụ thể còn thiếu; không áp pinout hoặc điện áp theo tên
gọi module một cách mặc định.

### 9.7. Điều kiện bắt đầu F2 và kiểm chứng bước này

F2 skeleton/compile có thể bắt đầu sau khi duyệt cách staging/build và các menu
board. F2 đọc cảm biến cần thêm GPIO, nguồn, điện áp tín hiệu, thư viện/driver
và hiệu chuẩn. F2 điều khiển bơm cần polarity, OFF khi boot/reset, nguồn/tải,
timer và điều kiện nước thấp/lỗi được xác nhận. Phần mạng còn phụ thuộc các
điểm hợp đồng/credential đã liệt kê ở mục 5 và 7.

Đã kiểm tra thông tin file cài đặt, CLI version, danh mục thư viện và Git.
Không compile, upload, mở Serial/COM10 hoặc điều khiển phần cứng; chưa có
logic production. Giữ nguyên hash hợp đồng BLE; Backend/Mobile/frontend và
esp32.ino không thay đổi. Không cài/nâng cấp dependency/core.

## 10. F1.2b-2 — Cấu hình dự kiến và skeleton

Các mục 6, 8, 9 ghi lại trạng thái ở các bước trước; trạng thái mới tại bước này
là đã có board_config.h và esp32.ino với setup/loop tối thiểu.

### 10.1. GPIO và hiệu chuẩn dự kiến

Cấu hình do người phụ trách cung cấp, khôi phục từ lịch sử; **chưa xác nhận
toàn bộ bằng sơ đồ điện thực tế**. Không suy ra nguồn, transistor hoặc polarity.

| Tín hiệu | GPIO dự kiến | Trạng thái |
|---|---|---|
| Soil Moisture AO | 34 | Chưa đối chiếu dây thực tế |
| DHT11 DATA | 27 | Chưa đối chiếu dây/pull-up |
| SRF05 TRIG | 18 | Chưa đối chiếu dây/mức logic |
| SRF05 ECHO | 19 | Chưa xác nhận chuyển mức |
| Relay IN1 | 23 | Chưa biết active HIGH/LOW; không cấu hình OUTPUT |

Soil dry ADC = **3230**, wet ADC = **2030**. Đây chỉ là giá trị hiệu chuẩn dự
kiến, chưa dùng để suy ra % hoặc điều khiển tưới; cần kiểm tra ADC resolution,
attenuation, nguồn module và hiệu chuẩn trên đất/cảm biến thực tế.

### 10.2. Skeleton và quy trình build

- board_config.h chỉ khai báo hằng GPIO/ADC, không thao tác GPIO.
- esp32.ino include board_config.h; setup chỉ Serial.begin(115200), in phiên
  bản `0.0.1-skeleton` và thông báo hardware control disabled.
- loop chỉ delay(1000), không đọc cảm biến, kết nối mạng, cấu hình chân hoặc
  điều khiển relay. Không chứa credential, không tạo module giả/rỗng hàng loạt.
- Build qua bản sao tạm `esp32/esp32.ino` cùng board_config.h, không đổi tên
  firmware/ và không tạo thêm primary sketch trong repository.
- Dùng CLI 1.5.1, core ESP32 3.3.12, FQBN esp32:esp32:esp32, các tùy chọn
  board mặc định của FQBN. Không bật chế độ debug riêng; chưa xác nhận flash,
  partition/PSRAM thực tế. Compile không chứng minh các tùy chọn này hợp board.

### 10.3. Nguồn, SRF05 và relay/transistor cần xác nhận

- Cần sơ đồ đường nguồn ESP32, soil, DHT11, SRF05, relay và bơm: nguồn nào cấp
  chân nào, điện áp đo thực tế, GND chung/cách ly, dòng tải và chống sụt áp.
- **SRF05 ECHO có nguy cơ mức 5 V:** chưa biết module và mức output thực tế;
  không nối ECHO trực tiếp vào GPIO19 khi chưa xác nhận. Cần mạch chia áp hoặc
  chuyển mức phù hợp nếu output vượt mức GPIO ESP32; kiểm tra cả TRIG.
- **Relay 12 V và transistor:** chưa xác nhận relay là module hay cuộn rời,
  nguồn 12 V đi đâu, driver tích hợp hay transistor ngoài. Không đưa 12 V vào
  GPIO23/3V3; không cấp cuộn relay trực tiếp từ GPIO. Nếu có transistor ngoài,
  cần mã linh kiện, pinout, cách mắc, điện trở điều khiển, bảo vệ tải cảm và
  polarity thực tế; không đoán chân B/C/E hoặc G/D/S từ hình dáng linh kiện.
- Chưa xác nhận relay active HIGH/LOW; không ghi mức logic, không đặt GPIO23
  thành OUTPUT. Phải kiểm tra OFF cả lúc cấp nguồn/reset trước khi cho nối bơm;
  việc skeleton không ghi GPIO không bảo đảm relay ngoài luôn OFF.
- Xác nhận COM/NO/NC, điện áp/dòng bơm và định mức tiếp điểm. Không tự cấp nguồn
  hoặc chạy bơm trong bước chuẩn bị.

### 10.4. Điều kiện trước khi chạy bơm/F2

1. Duyệt sơ đồ nguồn/GND, GPIO thực tế, chuyển mức ECHO và relay driver/polarity.
2. Kiểm tra relay OFF khi boot/reset/mất nguồn logic với tải bơm ngắt rời.
3. Kiểm chứng cảm biến, hình học bồn, hiệu chuẩn soil và phát hiện lỗi/timeout.
4. Triển khai timer cục bộ và chặn/dừng tưới khi nước thấp hoặc lỗi; không chạy
   bơm chỉ dựa vào các giá trị GPIO/ADC khôi phục từ lịch sử.
5. Chốt mâu thuẫn phút/giây và hành vi điều khiển trước tích hợp mạng; xác nhận
   cấu hình build thực tế và kiểm thử có giám sát trước khi tăng thời gian tưới.

### 10.5. Kết quả kiểm chứng

- Compile mặc định **PASS** bằng CLI 1.5.1/core 3.3.12/FQBN esp32:esp32:esp32.
  Flash 268972/1310720 byte (20%); RAM tĩnh 22116/327680 byte (6%).
- Staging: `C:/Users/nguye/AppData/Local/Temp/watering-skeleton-7765e840826446c6904efa2f384908ef/esp32`;
  output trong thư mục build cạnh sketch, không đưa binary vào repository.
- Lần thử sandbox lỗi Access denied ở Arduino15 và không nhận core; CLI cũng
  báo thử khởi tạo builtin tools nhưng bị chặn quyền. Chạy lại cùng lệnh compile
  ngoài sandbox thành công; không chạy core install/lib install hoặc nâng cấp.
- Không upload, mở COM10, chạy Serial Monitor hoặc kiểm thử phần cứng. PASS
  compile không phải firmware đã chạy trên ESP32 hoặc relay đã an toàn.
- Kiểm tra diff/whitespace; Backend/Mobile/frontend và hợp đồng BLE không đổi.
  Giữ thay đổi F1.2a/F1.2b-1, không commit/push.
