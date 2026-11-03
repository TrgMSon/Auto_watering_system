# Cài Watering App trên Android

App tưới cây tự động, bản thử nghiệm. App chưa có trên CH Play nên bạn cài trực tiếp từ file APK, mất khoảng 2 phút.

## Cần có trước khi đăng nhập

- Điện thoại bắt Wi-Fi giống với máy tính đang chạy máy chủ của app.
- Máy chủ đang bật.
- Tài khoản đăng nhập do người gửi app cung cấp.

Chưa có máy chủ thật thì dùng [máy chủ ảo](#dùng-máy-chủ-ảo-để-thử-app).

## Các bước cài đặt

1. **Tải file APK.** Mở link này trên điện thoại: [tải watering-app.apk](https://github.com/TrgMSon/Auto_watering_system/releases/download/android-latest-mobile_app/watering-app.apk). Không cần tài khoản GitHub. File thường nằm trong mục **Tải xuống** của điện thoại.
2. **Mở file và cho phép cài.** Chạm vào file APK. Nếu điện thoại chặn, chọn **Cài đặt** rồi bật **Cho phép từ nguồn này**, sau đó quay lại. Tên mục có thể hơi khác tùy hãng điện thoại, ví dụ "Cài ứng dụng không rõ nguồn gốc".
3. **Xác nhận cài đặt.** Chọn **Cài đặt**. Nếu Play Protect cảnh báo, chọn **Thông tin chi tiết** rồi **Vẫn cài đặt**. Cảnh báo này xuất hiện vì app không tải từ CH Play. Chỉ tiếp tục nếu file đến từ người bạn tin tưởng.
4. **Mở app và đăng nhập.** Tìm app có tên **watering_app** rồi đăng nhập bằng tài khoản được cung cấp.

## Nếu gặp lỗi

**Không đăng nhập được hoặc báo lỗi kết nối**

Kiểm tra điện thoại và máy chủ có cùng Wi-Fi không, rồi nhờ người gửi app xác nhận máy chủ đang bật. Nếu vẫn lỗi, có thể địa chỉ máy chủ đã đổi và bạn cần bản APK mới.

**Báo "Ứng dụng chưa được cài đặt" khi cập nhật**

Gỡ bản watering_app cũ rồi cài lại file mới. Sau đó bạn cần đăng nhập lại.

**Không thấy file sau khi tải**

Mở app **Tệp** hoặc **Files** trên điện thoại, vào mục **Tải xuống**.

## Dùng máy chủ ảo để thử app

Máy chủ ảo là script `mobile/tool/fake_backend.dart`, chạy trên máy tính và trả dữ liệu giả, không cần database, MQTT hay ESP32. Dữ liệu mất khi tắt script. Chỉ dùng để thử, vì mật khẩu test đã công khai trong repo.

Phần này dành cho người chạy máy chủ, cần máy tính đã cài Git và Flutter.

### 1. Chạy máy chủ

Script đang nằm ở nhánh `mobile_app`:

```bash
git clone -b mobile_app https://github.com/TrgMSon/Auto_watering_system.git
```

```bash
cd Auto_watering_system/mobile
```

```bash
dart tool/fake_backend.dart 8081 --lan
```

Giữ cửa sổ này mở trong lúc dùng app. Không có `--lan` thì điện thoại không gọi được. Nếu Windows hỏi quyền tường lửa, chọn **Allow** cho mạng Private.

### 2. Kiểm tra địa chỉ trong APK

Script in ra nhiều dòng `LAN (...)`, hãy lấy dòng của mạng đang dùng, ví dụ:

```text
LAN (Wi-Fi): http://192.168.61.102:8081/api
```

Địa chỉ này phải trùng với địa chỉ đã build vào APK. Nếu khác, người có quyền ghi trên repo làm như sau rồi cài lại APK:

1. Vào **Settings → Secrets and variables → Actions → Variables** và sửa `API_BASE_URL`.
2. Vào **Actions → Android build (APK) → Run workflow**, chờ build xong.

IP của máy tính có thể đổi khi đổi Wi-Fi hoặc kết nối lại. Khi đó phải làm lại hai bước trên.

### 3. Kiểm tra điện thoại gọi được máy chủ

Bắt cùng Wi-Fi với máy tính rồi mở bằng trình duyệt trên điện thoại, thay IP bằng IP của bạn:

```text
http://192.168.61.102:8081/api/devices
```

Thấy dòng `Phiên đăng nhập không hợp lệ.` là đã kết nối được. Nếu trang không tải được, kiểm tra lại Wi-Fi và tường lửa. Với tường lửa Windows, mở PowerShell bằng quyền Administrator và chạy:

```bash
netsh advfirewall firewall add rule name="Watering fake backend" dir=in action=allow protocol=TCP localport=8081
```

### 4. Đăng nhập

Mật khẩu của cả ba tài khoản là `123456`.

| Tài khoản | Dùng để thử |
| --- | --- |
| `user@test.com` | Có 5 thiết bị mẫu |
| `empty@test.com` | Chưa có thiết bị nào |
| `admin@test.com` | Tài khoản admin, bị chặn ở các API thiết bị |

### 5. Các thiết bị mẫu

| Thiết bị | Kết quả khi thử |
| --- | --- |
| Vườn 1 | Bật bơm thành công |
| Vườn 2 | Offline, không điều khiển được |
| Vườn 3 | Thiết bị từ chối lệnh bơm |
| Vườn 4 | Chưa có dữ liệu cảm biến |
| Vườn 5 | Thiết bị không phản hồi, app báo hết thời gian chờ |

### 6. Giả lập tình huống

Gõ lệnh vào cửa sổ đang chạy script rồi nhấn Enter:

| Lệnh | Tác dụng |
| --- | --- |
| `status` | Xem trạng thái thiết bị và phiên đăng nhập |
| `offline 1` | Cho thiết bị 1 offline (số từ 1 đến 5) |
| `online 1` | Cho thiết bị 1 online lại |
| `expire` | Làm access token hết hạn, app phải tự làm mới |
| `revoke` | Thu hồi phiên, app phải đăng xuất |
| `down` | Giả lập mất máy chủ |
| `up` | Khôi phục máy chủ |
| `help` | Xem danh sách lệnh |
| `quit` | Tắt máy chủ |
