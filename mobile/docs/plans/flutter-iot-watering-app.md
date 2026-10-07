# 🌿 Smart Watering — Kế hoạch Thiết kế UI (Mobile App)

**Mục tiêu:** Tập trung thiết kế giao diện (UI) cho ứng dụng IoT tưới cây tự động bằng Flutter. Phần Backend (Java Spring Boot) sẽ do team khác đảm nhận, nên ứng dụng Mobile hiện tại sẽ sử dụng **Dữ liệu giả (Mock Data)** để hiển thị và tương tác trực quan.

**Công nghệ UI:** Flutter (Material 3), `go_router` (chuyển trang), `flutter_bloc` (quản lý trạng thái UI), `fl_chart` (vẽ biểu đồ).

---

## 📱 Danh sách các Màn hình (Screens)

### 1. Màn hình1 Đăng nhập (Login Page)
- **Vị trí:** `lib/features/auth/presentation/pages/login_page.dart`
- **Thành phần UI:**
  - Logo/Icon hệ thống tưới cây.
  - Textfield Nhập Username và Password.
  - Nút "Đăng nhập".
- **Mock Logic:** Nhập bất kỳ thông tin nào cũng sẽ báo đăng nhập thành công và chuyển vào màn hình chính.

### 2. Màn hình Tổng quan (Dashboard Page)
- **Vị trí:** `lib/features/dashboard/presentation/pages/dashboard_page.dart`
- **Thành phần UI:**
  - Card Bật/Tắt chế độ "Tưới tự động" (có Switch).
  - 4 Card nhỏ hiển thị chỉ số cảm biến giả lập:
    - 💧 Độ ẩm đất (ví dụ: 45%)
    - 🌡️ Nhiệt độ (ví dụ: 28°C)
    - ☁️ Độ ẩm không khí (ví dụ: 60%)
    - ☀️ Ánh sáng (ví dụ: 1000 lux)
  - Thông báo trạng thái: Có bao nhiêu thiết bị đang online/offline.

### 3. Màn hình Thống kê Biểu đồ (Sensor Monitor Page)
- **Vị trí:** `lib/features/sensor_monitor/presentation/pages/sensor_monitor_page.dart`
- **Thành phần UI:**
  - Card hiển thị dữ liệu realtime (nhảy số liên tục bằng Timer mô phỏng).
  - 3 Biểu đồ dạng đường (Line Chart dùng `fl_chart`) để vẽ lại lịch sử 24h của: Nhiệt độ, Độ ẩm đất, Độ ẩm không khí.

### 4. Màn hình Quản lý Thiết bị (Device List Page)
- **Vị trí:** `lib/features/device_management/presentation/pages/device_list_page.dart`
- **Thành phần UI:**
  - Danh sách (ListView) các thiết bị phần cứng (Máy bơm, Van nước, Cảm biến).
  - Trạng thái thiết bị: Online (Màu xanh), Offline (Màu xám).
  - Switch: Bật/tắt thủ công máy bơm hoặc van nước.
  - Floating Action Button: Nút (+) để thêm thiết bị ảo.

### 5. Màn hình Cài đặt & Hồ sơ (Profile & Settings Page)
- **Vị trí:** `lib/features/profile/presentation/pages/profile_page.dart`
- **Thành phần UI:**
  - Thông tin người dùng đang đăng nhập (Avatar, Tên, Email, Quyền).
  - (Nếu là Admin) -> Hiển thị menu "Quản lý người dùng".
  - Nút Đăng xuất.

### 6. Màn hình Quản lý Người dùng (Admin Only)
- **Vị trí:** `lib/features/user_management/presentation/pages/user_management_page.dart`
- **Thành phần UI:**
  - Danh sách tài khoản trong hệ thống.
  - Tính năng cấp quyền Admin/User.

---

## 🛠 Lộ trình Triển khai Code UI (Mock Data)

Vì không cần kết nối Backend ngay, cấu trúc code sẽ được tinh gọn để tập trung vào UI:

1. **Setup Theme & Router:** Đã cấu hình `AppTheme` (màu sắc cây cỏ) và `GoRouter` (chuyển tab dưới đáy màn hình).
2. **Setup Mock Data:** Thay thế các Repository gọi API (`Dio`, `WebSocket`) bằng các Class trả về dữ liệu cố định (Fake Data) hoặc dùng `Timer` để giả lập dữ liệu realtime nhảy số.
3. **Hoàn thiện giao diện:** Code chi tiết các Widget (Nút bấm, Biểu đồ, Card) bằng Material 3.
4. **Liên kết BLoC:** BLoC sẽ gọi Mock Data và cập nhật UI ngay lập tức để bạn có thể bấm thử (ví dụ: Bấm bật máy bơm -> Máy bơm chuyển sang màu xanh).

*(Toàn bộ các phần cấu hình API thừa, Token, STOMP WebSocket đã được gỡ bỏ khỏi kế hoạch này để bạn tập trung hoàn toàn vào Thiết kế Trải nghiệm Người dùng - UX/UI).*
