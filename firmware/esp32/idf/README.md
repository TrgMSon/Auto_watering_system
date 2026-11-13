# Build firmware bằng ESP-IDF và Arduino framework

Dự án này build sketch hiện tại `firmware/esp32/esp32.ino` với Arduino-ESP32
3.3.12 dưới dạng một component của ESP-IDF. Cách build này cho phép đặt các
tùy chọn bộ đệm mbedTLS trong `sdkconfig.defaults`; Arduino IDE Boards Manager
không áp dụng được các tùy chọn đó.

## Yêu cầu

- ESP-IDF 5.5.5 và toolchain ESP32, cài bằng ESP-IDF Tools Installer của Espressif.
- Hai thư viện Arduino: `DHT sensor library` và `Adafruit Unified Sensor`.
- File `firmware/esp32/wifi_local.h` trên máy, chứa cấu hình Wi-Fi cố định.
- Board ESP32 có flash tối thiểu 4 MB. Partition table đi kèm dành hai phân vùng
  OTA, mỗi phân vùng 1.875 MB, và đặt NVS tại offset chuẩn `0x9000`.

Mở **ESP-IDF 5.5.5 PowerShell** rồi chuyển đến thư mục dự án. Ở lần thiết lập
đầu tiên, cần chỉ định đường dẫn đến hai thư viện chứa `DHT.cpp` / `DHT.h` và
`Adafruit_Sensor.cpp` / `Adafruit_Sensor.h`. Với Arduino sketchbook mặc định,
các thư viện thường nằm trong `Documents/Arduino/libraries/DHT_sensor_library`
và `Documents/Arduino/libraries/Adafruit_Unified_Sensor`.

Giữ partition table tương thích với firmware Arduino đang cài trên board; đổi
bố cục partition có thể làm NVS cũ không còn truy cập được.

## Định danh thiết bị

Firmware đọc MAC Wi-Fi STA từ địa chỉ phần cứng của ESP32 và không ghi đè MAC.
Định danh MQTT/BLE được tạo từ MAC của từng board, vì vậy hãy đăng ký đúng ID
thực tế của board ở backend và provision thông tin MQTT/BLE key tương ứng; các
bản ghi đã provision theo ID khác sẽ không được chấp nhận.

## Thiết lập lần đầu

Chạy một lần cho thư mục build này. Thay hai đường dẫn thư viện bằng đường dẫn
thực tế trên máy:

```powershell
cd firmware/esp32/idf
idf.py -DDHT_SENSOR_LIBRARY_DIR="C:/path/to/DHT_sensor_library" `
       -DADAFRUIT_UNIFIED_SENSOR_LIBRARY_DIR="C:/path/to/Adafruit_Unified_Sensor" `
       set-target esp32
```

Lệnh `set-target` khởi tạo cấu hình build cho ESP32. Ở lần cấu hình đầu tiên,
ESP-IDF Component Manager sẽ tải Arduino component đã được khóa phiên bản.

## Build và nạp các lần sau

Sau khi sửa firmware, build và nạp từ cùng thư mục dự án:

```powershell
idf.py build
idf.py -p COM9 flash monitor
```

Thay `COM9` bằng cổng serial của board. Không chạy lại `set-target` trong các
lần build thông thường vì lệnh này khởi tạo lại cấu hình build. Project tự chạy
các hàm Arduino `setup()` và `loop()`.

Giới hạn record TLS đầu vào được đặt là 8 KB thay vì mức chuẩn tối đa 16 KB để
giảm RAM. Nếu broker gửi record TLS lớn hơn giới hạn này, quá trình bắt tay TLS
vẫn có thể thất bại; hãy kiểm tra log serial sau khi nạp firmware.
