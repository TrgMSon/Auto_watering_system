# Câu hỏi về BLE cần chốt với Hải

**Phiên bản 04/11/2026 — cập nhật theo 3 quyết định đã được cả nhóm xác nhận.** Đây là hợp đồng triển khai giữa ESP32, Mobile App và Backend. Các vấn đề kỹ thuật còn cần kiểm tra được đánh dấu rõ; quyết định giao thức không đồng nghĩa với tính năng đã có trong source.

## 1. UUID

**1. Dùng chung một service UUID cho mọi thiết bị được không?**

- Đề xuất: dùng chung, không để admin gán riêng từng thiết bị, vì backend không có trường này.
- Trả lời: Đồng ý. Một Service UUID dùng chung cho mọi ESP32; phân biệt từng thiết bị bằng mã định danh MAC Wi-Fi, không dùng UUID riêng theo thiết bị.

**2. UUID thật là gì, và ai tạo?**

- **CHỐT:** Firmware công bố một bộ UUID 128-bit cố định, chung cho mọi thiết bị. Service: `07975e87-679e-531b-b48b-d227f4570000`; characteristics `07975e87-679e-531b-b48b-d227f4570001`, `...0002`, `...0003`, `...0004`, `...0005`, `...0006`, `...0007` (các dấu `...` chỉ là viết tắt trong tài liệu; trong code phải dùng chuỗi đầy đủ theo cùng mẫu). Đây là UUID hợp lệ, tạo từ định danh dự án, không phải UUID v4 ngẫu nhiên. Mobile và firmware không tự thay đổi bộ UUID sau khi thống nhất.

## 2. Nhận diện thiết bị

**3. Tên quảng bá BLE đặt thế nào?**

- Đề xuất: `WS-383E516FC7A8`, đặt trong scan response; service UUID đặt trong gói quảng bá.
- Trả lời: Đồng ý tên `WS-<MAC12>` (ví dụ `WS-383E516FC7A8`). Ưu tiên đưa tên vào scan response, service UUID vào advertising nếu kích thước gói cho phép; app phải có phương án nhận diện qua service và đọc device ID vì không thể mặc định mọi điện thoại đều hiển thị đầy đủ tên trong scan.

**4. Dùng MAC nào để nhận diện?**

- Đề xuất: MAC Wi-Fi, trùng với MAC nhập trên Web Admin, và in ra Serial để đối chiếu.
- **Trả lời — ĐÃ CHỐT VÀ KIỂM TRA TRÊN ESP32:** Dùng MAC Wi-Fi STA của ESP32, không dùng địa chỉ BLE ngẫu nhiên. MAC đọc thực tế từ Serial: `38:3E:51:6F:C7:A8`. In MAC khi khởi động để đối chiếu Web Admin.

**5. Mã thiết bị trong BLE và MQTT viết thế nào?**

- **Trả lời — ĐÃ CHỐT CẢ NHÓM:** Device ID chính thức là MAC Wi-Fi STA **6 cặp hex viết hoa có dấu `:`**, ví dụ của ESP32 hiện tại: `38:3E:51:6F:C7:A8`. Dùng đúng chuỗi này trong Web Admin, Backend, định danh MQTT topic và trường `device_id` khi có. Riêng BLE advertising name bỏ dấu `:`: `WS-383E516FC7A8`. Mobile chuyển từ tên BLE sang MAC có dấu `:` khi gọi API; không dùng `383E516FC7A8` làm ID chính thức trong Backend/MQTT.
- MQTT ví dụ: `iot/garden/38:3E:51:6F:C7:A8/telemetry`, `/cmd`, `/config`, `/status` (cần lấy chính xác các topic bổ sung như `/ack` từ code Backend).

**6. Khi mất kết nối, ESP32 có tự quảng bá lại không? Một lúc cho mấy điện thoại kết nối?**

- Đề xuất: tự quảng bá lại, một điện thoại mỗi lúc.
- Trả lời: Đồng ý tự advertising lại sau disconnect, tối đa 1 kết nối Mobile đồng thời. Không ngừng logic tưới tự động khi BLE ngắt.

## 3. Định dạng dữ liệu

**7. MTU đặt bao nhiêu?**

- Đề xuất: app xin MTU 185, tức tối đa 182 byte dữ liệu mỗi gói.
- Trả lời: Đồng ý Mobile yêu cầu ATT MTU 185; payload tối đa 182 byte **chỉ khi đàm phán MTU thành công**. Firmware kiểm tra MTU thực tế; nếu nhỏ hơn phải rút gọn/chia gói theo quy ước (không mặc định 182 byte).

**8. Gói cảm biến `0001` dùng dạng này được không? Cảm biến lỗi thì gửi gì?**

- Đề xuất: `{"soil":42.5,"temp":31.8,"hum":68.0,"water":25.4,"pump":0}`, đơn vị `%`, `°C`, `%`, `cm`; `pump` là 0 hoặc 1; cảm biến lỗi gửi `null`.
- Trả lời: Đồng ý JSON `{"soil":42.5,"temp":31.8,"hum":68.0,"water":25.4,"pump":0}`; soil %, temp °C, hum %, water cm; pump 0/1. Cảm biến lỗi trả `null` tại trường tương ứng, không thay bằng 0. `0001` hỗ trợ READ và NOTIFY (chu kỳ mục tiêu 2 giây).

**9. Lệnh bơm `0002` dùng dạng này được không? `dur` tối đa bao nhiêu?**

- Đề xuất: `{"cmd":"PUMP_ON","dur":300,"sig":"..."}` và `{"cmd":"PUMP_OFF","dur":0,"sig":"..."}`, `dur` tính bằng giây, giới hạn tối đa theo cấu hình tưới.
- Trả lời: Đồng ý `PUMP_ON`/`PUMP_OFF`; `dur` là số nguyên giây. `PUMP_OFF` bắt buộc `dur=0`. `PUMP_ON` yêu cầu `1 <= dur <= max_watering_duration_minutes*60` và không vượt giới hạn an toàn cấu hình; mặc định **2 phút (120 giây)**; cấu hình giới hạn từ **60 đến 600 giây** (1–10 phút). `dur` của lệnh bật bơm là số giây nguyên từ 1 đến giới hạn cấu hình hiện hành; không đồng nghĩa với giới hạn thử nghiệm 5 giây hiện tại. Khi mực nước thấp, từ chối bật bơm.

**10. Cấu hình `0003` có cần thêm trường nào không?**

- Đề xuất: `{"min":30.0,"max":70.0,"auto":1,"sig":"..."}`, cân nhắc thêm thời gian tưới tối đa và ngưỡng mực nước vì MQTT đang có hai trường này.
- Trả lời: Đề xuất mở rộng `0003` thành `{"min":30.0,"max":70.0,"auto":1,"max_dur":600,"min_water":5.0,"sig":"..."}`. `max_dur` tính giây, `min_water` tính **cm** (chốt hợp đồng BLE). Firmware chuyển đổi số đo SRF05 sang chiều cao cột nước tính bằng cm; Backend cần đồng bộ đơn vị MQTT theo tài liệu. Không gửi giá trị % vào trường này. Yêu cầu `0 <= min < max <= 100`, `auto` 0/1, `60 <= max_dur <= 600`. Cập nhật quy tắc ký tại câu 12 để bao gồm cả hai trường mới.

**11. App ghi lệnh bằng Write có phản hồi được không?**

- Đề xuất: có, cho cả `0002` và `0003`.
- Trả lời: Đồng ý `0002` và `0003` dùng Write With Response ở tầng GATT; phản hồi GATT chỉ xác nhận đã nhận bản ghi, còn thành công/thất bại nghiệp vụ trả qua Notify `0005`.

## 4. Chữ ký `sig`

**12. Chuỗi nào được ký?**

- Đề xuất: bật/tắt bơm ký `nonce|cmd|dur`; cấu hình ký `nonce|CONFIG|min|max|auto`.
- Trả lời: Lệnh bơm: `nonce|cmd|dur`. Cấu hình mở rộng: `nonce|CONFIG|min|max|auto|max_dur|min_water`, đúng thứ tự, không khoảng trắng; ví dụ `nonce|CONFIG|30.0|70.0|1|120|5.0`. Hai trường bổ sung bắt buộc có trong chữ ký để tránh sửa nội dung không bị phát hiện. **Vector CONFIG cũ tại câu 15 chỉ là vector lịch sử cho cấu hình 3 trường, không dùng để kiểm tra giao thức đã chốt.**

**13. Tạo chữ ký và dùng khóa thế nào?**

- Đề xuất: lấy 16 ký tự hex đầu của HMAC-SHA256; dùng nguyên chuỗi `ble_secret_key` mã hóa UTF-8, không giải mã Base64; số thực luôn có một chữ số thập phân.
- Trả lời: Đồng ý HMAC-SHA256(key=UTF-8 của `ble_secret_key`, message=UTF-8 của chuỗi câu 12), lấy 16 ký tự hex thường đầu tiên của digest (64 bit). `min`, `max`, `min_water` định dạng đúng 1 chữ số thập phân; `dur`, `max_dur`, `auto` là số nguyên thập phân, không khoảng trắng. Đây là quy ước tương thích của đồ án; việc cắt HMAC còn 64 bit cần được cân nhắc nếu triển khai thực tế.

**14. Quy tắc nonce thế nào?**

- Đề xuất: 8 byte ngẫu nhiên viết thành 16 ký tự hex thường, dùng một lần, hết hạn sau 30 giây; sau mỗi lần ghi lệnh, nonce cũ mất hiệu lực.
- Trả lời: Đồng ý nonce ngẫu nhiên 8 byte (16 hex thường), hết hạn 30 giây, dùng một lần. Mỗi lần WRITE có xác thực (`0002`/`0003`/`0006`/`0007`) phải có nonce mới; tiêu thụ nonce kể cả khi chữ ký/lệnh không hợp lệ để tránh thử lại. App READ `0004` trước từng lệnh. Firmware dùng nguồn ngẫu nhiên bảo mật của ESP32.

**15. ESP32 có tính ra đúng bộ dữ liệu mẫu dưới đây không?**

Khóa thử, chỉ dùng để kiểm tra: `test-ble-secret-key`

| Chuỗi cần ký | `sig` đúng |
| --- | --- |
| `84a9f21b3c0e5a1d\|PUMP_ON\|300` | `48ba618920e3dcb4` |
| `84a9f21b3c0e5a1d\|PUMP_OFF\|0` | `238041d3721ef169` |
| `84a9f21b3c0e5a1d\|CONFIG\|30.0\|70.0\|1` | `9c77373f7323c7ac` |

- Đề xuất: Hải chạy thử và đối chiếu cả 3 dòng.
- **Trả lời — ĐÃ CHỐT (03/03 PASS trên ESP32):** Đã nạp và chạy chương trình kiểm thử trực tiếp trên ESP32 bằng mbedTLS HMAC-SHA256. Serial Monitor ghi nhận `PUMP_ON: PASS` (actual = expected = `48ba618920e3dcb4`), `PUMP_OFF: PASS` (`238041d3721ef169`) và `CONFIG` định dạng 3 trường: PASS (`9c77373f7323c7ac`). Chốt quy tắc HMAC-SHA256 với khóa UTF-8, chuỗi ký UTF-8 đúng thứ tự và `sig` là 16 ký tự hex thường đầu tiên của digest. **Phạm vi kiểm thử:** 3 vector gốc đã chạy trên ESP32; chưa kiểm thử giao tiếp BLE, xác minh nonce, hoặc vector CONFIG mở rộng trên ESP32. Dòng CONFIG gốc chỉ áp dụng định dạng 3 trường ban đầu. **Vector cho cấu hình mở rộng:** `84a9f21b3c0e5a1d|CONFIG|30.0|70.0|1|600|5.0` → `1e4b0b9e4f4d0ea5` (vector tương thích lịch sử; `600` vẫn hợp lệ, dù mặc định mới là `120`) (đã tính độc lập, còn cần chạy trên ESP32).

## 5. Kết quả lệnh

**16. Thêm characteristic `0005` loại Notify để trả kết quả lệnh được không?**

- Đề xuất: có, dạng `{"op":"PUMP_ON","code":"OK"}`.
- Trả lời: Đồng ý thêm `0005` Notify, ví dụ `{"op":"PUMP_ON","code":"OK"}`. Nên gửi kết quả sau khi kiểm tra chữ ký, giới hạn và trạng thái bơm; thông báo `OK` chỉ khi lệnh đã được chấp nhận/xử lý.

**17. Bảng mã lỗi này đã đủ chưa?**

- Đề xuất: `OK`, `BAD_SIG`, `NONCE_EXPIRED`, `BAD_JSON`, `OUT_OF_RANGE`, `LOW_WATER`, `MAINTENANCE`.
- **Trả lời — ĐÃ CHỐT:** Giữ 7 mã ban đầu: `OK`, `BAD_SIG`, `NONCE_EXPIRED`, `BAD_JSON`, `OUT_OF_RANGE`, `LOW_WATER`, `MAINTENANCE`. **Bổ sung đúng 3 mã:** `SENSOR_ERROR` (không đọc được cảm biến cần thiết), `PUMP_BUSY` (lệnh xung đột khi bơm đang hoạt động), `INVALID_TIME` (timestamp đồng bộ qua `0007` không hợp lệ). **Bổ sung 2 mã đã được cả nhóm chốt:** `WIFI_OK` (kết nối Wi-Fi thành công), `WIFI_FAIL` (không kết nối được trong thời hạn 30 giây). **Tổng cộng 12 mã kết quả cố định.** Không sử dụng các mã dự kiến trước đây là `BUSY` hoặc `UNAUTHORIZED`; dùng chính xác `PUMP_BUSY`. Lệnh `PUMP_OFF` vẫn phải được xử lý khi bơm đang chạy, không bị từ chối vì `PUMP_BUSY`.

**18. Quy trình gửi lệnh thế này được không?**

- Đề xuất: app bật Notify `0005` trước, mỗi lúc gửi một lệnh, chờ tối đa 3 giây.
- Trả lời: Đồng ý app đăng ký Notify `0005` trước, chỉ gửi một lệnh đang chờ tại một thời điểm, timeout 3 giây. Nếu timeout, đọc nonce mới và kiểm tra trạng thái trước khi gửi lại; tránh tự động lặp `PUMP_ON` một cách mù quáng.

## 6. RTC

**19. "Có RTC" là RTC trong chip hay module rời có pin?**

- Đề xuất: nếu chỉ có RTC trong chip thì thêm `0007` để app gửi giờ qua BLE sau mỗi lần kết nối. Nếu là module rời có pin thì cần xem lại có còn cần `0007` không.
- Trả lời: Theo linh kiện đã xác nhận, **chưa có module RTC rời có pin**; dùng đồng hồ trong ESP32, thời gian có thể mất sau khi tắt nguồn. Đề xuất thêm `0007` Write With Response để Mobile đồng bộ Unix timestamp UTC theo giây khi kết nối, ví dụ `{"unix":1793800000,"sig":"..."}` với chữ ký `nonce|TIME|unix` (nonce đọc từ `0004`). Khi có Wi-Fi có thể đồng bộ NTP. **Chốt `0007` và định dạng này**; Mobile cần triển khai cùng hợp đồng. Vector kiểm thử: `84a9f21b3c0e5a1d|TIME|1793800000` → `26b87998f4bc4778` với khóa thử ở câu 15. ESP32 chỉ chấp nhận thời gian sau khi xác thực hợp lệ.


## 7. Cấu hình Wi-Fi qua BLE — bổ sung đã chốt (Characteristic `0006`)

**20. Có hỗ trợ nhập SSID và mật khẩu Wi-Fi từ Mobile App không?**

- **ĐÃ CHỐT CẢ NHÓM:** Có. Characteristic `07975e87-679e-531b-b48b-d227f4570006`, quyền **Write With Response**. Payload UTF-8 JSON: `{"ssid":"HomeWiFi","pass":"12345678","sig":"..."}`; các giá trị là ví dụ, không phải mật khẩu thật.
- Chuỗi ký HMAC-SHA256: `nonce|WIFI|ssid|pass`. Đọc nonce mới từ `0004` trước mỗi WRITE; dùng quy tắc khóa và chữ ký 16 hex thường như câu 13. SSID/password được ký theo đúng chuỗi ký tự UTF-8 thực tế; ký tự `|` trong SSID/password cần quy tắc escape/đóng khung trước khi triển khai, **chưa chốt** (không tự ý bỏ qua).
- **Bảo mật bắt buộc:** Chỉ nhận mật khẩu qua liên kết BLE đã pairing/bonding và mã hóa, có kiểm tra trạng thái bảo mật ở ESP32; HMAC không mã hóa mật khẩu. Không ghi mật khẩu ra Serial/log, không lưu plaintext trong repository.
- Sau khi xác thực chữ ký, ESP32 thử kết nối Wi-Fi tối đa **30 giây**. Kết quả nghiệp vụ gửi qua Notify `0005`: `{"op":"WIFI","code":"WIFI_OK"}` hoặc `{"op":"WIFI","code":"WIFI_FAIL"}`. GATT Write Response không có nghĩa là đã kết nối Wi-Fi thành công.
- **Chỉ lưu Wi-Fi credentials vào NVS khi kết nối thành công**; nếu thất bại, giữ cấu hình Wi-Fi đã lưu trước đó (nếu có). Tiếp tục quảng bá BLE để người dùng thử lại.
- Mobile bật Notify `0005` trước khi ghi `0006`; ngoại lệ thời gian chờ: lệnh Wi-Fi có thể mất **tối đa 30 giây**, không áp dụng timeout 3 giây của lệnh bơm/cấu hình. Nên có thêm biên độ nhỏ cho truyền Notify, và không gửi lệnh thứ hai khi đang provisioning.
- **Còn phải kiểm tra khi lập trình:** kích thước JSON so với MTU thực tế; nếu không vừa một lần Write, hai bên phải thống nhất cơ chế ghi dài/chia gói. Chưa coi phân mảnh là đã hỗ trợ.

## 8. Quy ước thời gian tưới — cập nhật đã chốt

- **Mobile UI và Backend:** dùng **phút**, tên trường Backend `max_watering_duration_minutes` khi thích hợp theo code thực tế.
- **BLE `0003`:** `max_dur` là **giây**. Mobile nhân số phút với 60 trước khi ký và gửi; ESP32 lưu và dùng giây.
- **Khoảng cấu hình:** 1–10 phút = 60–600 giây. **Mặc định:** 2 phút = 120 giây.
- **BLE `0002`:** `dur` là số giây của lệnh chạy bơm, không được vượt `max_dur` đang lưu. `PUMP_OFF` luôn `dur=0`.
- **MQTT:** firmware parse đơn vị và tên trường theo publisher thực tế của Backend, không suy diễn từ BLE; cần có bài test chuyển đổi đơn vị khi tích hợp.

## 9. Bảng GATT và kết quả chính thức

| Hậu tố UUID | Vai trò | Quyền |
|---|---|---|
| `0000` | Service chung | Service |
| `0001` | Sensor + trạng thái bơm | Read, Notify |
| `0002` | Pump command | Write With Response |
| `0003` | Device config | Write With Response |
| `0004` | Nonce | Read |
| `0005` | Kết quả lệnh | Notify |
| `0006` | Wi-Fi provisioning | Write With Response; yêu cầu BLE bảo mật |
| `0007` | Đồng bộ Unix time UTC | Write With Response |

Mọi UUID dùng tiền tố `07975e87-679e-531b-b48b-d227f457` và hậu tố 4 chữ số ở bảng. Danh sách **12 mã kết quả**: `OK`, `BAD_SIG`, `NONCE_EXPIRED`, `BAD_JSON`, `OUT_OF_RANGE`, `LOW_WATER`, `MAINTENANCE`, `SENSOR_ERROR`, `PUMP_BUSY`, `INVALID_TIME`, `WIFI_OK`, `WIFI_FAIL`.

## 10. Phân biệt quyết định đã chốt với việc cần xác minh

- **Đã kiểm thử trên ESP32:** MAC Wi-Fi STA `38:3E:51:6F:C7:A8`; 3 vector HMAC gốc PASS. **Chưa kiểm thử:** BLE pairing/bonding, `0006`, Wi-Fi provisioning, nonce thực tế, HMAC CONFIG mở rộng, TIME, WIFI, MQTT và OTA.
- **Cần xác minh Backend trước khi dùng secret thực:** Backend sinh khóa dạng Base64URL; hợp đồng hiện quy định dùng **UTF-8 của chuỗi `ble_secret_key`** làm HMAC key. Phải xác nhận đúng giá trị chuỗi được Mobile nhận và ESP32 được nạp, cùng cách cấp khóa ban đầu; không tự ý decode Base64URL.
- **Cần xác minh trước khi nhận Wi-Fi password:** cách pairing/bonding và yêu cầu mã hóa GATT trên cả Mobile/ESP32; cách xử lý ký tự `|` trong SSID/password; giới hạn MTU/long write. Nếu chưa hoàn tất, không bật provisioning nhận mật khẩu thật.
- **Cần xác minh khi tích hợp MQTT:** payload ACK, schema telemetry, topic `/ack`, TLS/credential và chính sách đồng bộ cấu hình; lấy từ code Backend đang chạy, không tự suy đoán.

## Kết luận và các điểm phối hợp

- **Đã chốt cả nhóm:** `device_id` chính thức là `38:3E:51:6F:C7:A8` (MAC có dấu `:`); tên BLE là `WS-383E516FC7A8`. Mực nước BLE dùng cm; cần đối chiếu trường `waterLevel`/`min_water_level_alert` trong code Backend khi tích hợp.
- **Đã chốt hợp đồng Mobile + firmware:** bộ UUID 128-bit nêu ở câu 2, có thêm `0006`; schema `0003` mở rộng, HMAC gồm đủ trường; đã có vector CONFIG mở rộng để đối chiếu.
- **Đã chốt hợp đồng Mobile + firmware:** `0007` đồng bộ thời gian có HMAC; `0006` cấu hình Wi-Fi; Notify `0005` báo kết quả; MTU 185 là mục tiêu, không phải bảo đảm. Hai bên cần kiểm thử MTU nhỏ, dữ liệu dài và lỗi/timeout.
- **Đã kiểm thử trên ESP32:** 3/3 HMAC test vectors gốc ở câu 15 PASS (PUMP_ON, PUMP_OFF, CONFIG 3 trường). **Chưa kiểm thử:** vector CONFIG mở rộng, TIME, xác minh nonce và lệnh BLE thực tế với relay/bơm.
- **Đã chốt câu 17:** dùng đúng 12 mã kết quả (`OK`, `BAD_SIG`, `NONCE_EXPIRED`, `BAD_JSON`, `OUT_OF_RANGE`, `LOW_WATER`, `MAINTENANCE`, `SENSOR_ERROR`, `PUMP_BUSY`, `INVALID_TIME`, `WIFI_OK`, `WIFI_FAIL`).

## Ghi chú triển khai an toàn

- Giới hạn mặc định `max_dur=120` giây (cấu hình được từ 60 đến 600 giây) là giới hạn cấu hình, không phải yêu cầu chạy bơm 600 giây khi demo. Giữ giới hạn 5 giây khi kiểm thử phần cứng, chỉ tăng sau khi xác minh nguồn, bơm và chống chạy khô.
- Đối với `0003`, gửi đủ 5 trường cấu hình; chưa hỗ trợ cập nhật từng phần. Sau khi xác thực thành công, kiểm tra hợp lệ rồi mới lưu NVS và phản hồi `OK`.
- Nếu MTU không đủ cho JSON một lần, hai bên phải thống nhất cơ chế chia gói hoặc dùng READ dài; **chưa được coi là đã có cơ chế phân mảnh**.
- Chữ ký HMAC rút gọn 64 bit được giữ để tương thích đồ án; không sử dụng khóa thử làm khóa sản xuất.
- Các quyết định về Wi-Fi BLE, đơn vị thời gian và định dạng Device ID **đã được nhóm đồng ý**; chưa có bằng chứng đã được triển khai và kiểm thử tích hợp trong firmware/Mobile/Backend.
