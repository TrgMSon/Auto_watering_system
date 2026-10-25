# Quyết định Kiến trúc Giao diện & Đồng bộ (Tab 2: Tự động hóa)
*Ngày tạo: 2026-10-07*

## 1. Tóm tắt Yêu cầu (Understanding Summary)
* **Mục tiêu:** Xây dựng Tab 2 (Automation) cho phép người dùng cấu hình ngưỡng cảm biến (`device_configs`) và thiết lập lịch hẹn giờ (`schedules`).
* **Chiến lược kết nối:** Hỗ trợ Dual Sync (Đồng bộ kép) giữa Bluetooth Low Energy (BLE) và HTTP Backend.
* **Quy trình:** Người dùng đổi cấu hình -> App bắn BLE xuống ESP32 ngay lập tức -> App gọi API Backend. Nếu mất mạng, lưu cấu hình vào bộ nhớ tạm để đẩy bù sau.

## 2. Các Giả định & Rủi ro (Assumptions & Risks)
* **Giả định:** 
  * App sử dụng cơ chế **ESP32-driven Sync (Hardware Queue)**. Khi kết nối BLE, App chỉ có nhiệm vụ bắn cấu hình xuống ESP32. ESP32 sẽ lưu vào EEPROM/SPIFFS và tự gọi API Backend khi có Wi-Fi.
  * Việc thêm/sửa lịch (`schedules`) gọi API `POST /api/v1/schedules`.
  * Lấy cấu hình tự động kẹp trong `GET /api/v1/devices` và cập nhật bằng `PUT /api/v1/devices/{id}/config`.
* **Rủi ro:** Không có rủi ro về Data drift trên mobile app vì app không còn lưu hàng đợi cục bộ. Tuy nhiên cần đảm bảo ESP32 lập trình tốt phần tự động sync khi reconnect Wi-Fi.

## 3. Nhật ký Quyết định (Decision Log)
1. **[Quyết định] Sử dụng mô hình ESP32-driven Sync thay vì Mobile Local Queue.**
   * *Lý do:* Giải phóng gánh nặng cho Mobile App. Đảm bảo cấu hình luôn đi theo phần cứng (kể cả khi đổi điện thoại khác).
2. **[Quyết định] Lỗi mạng khi điều khiển từ xa (không có BLE) sẽ throw thẳng lỗi cho UI.**
   * *Lý do:* Giữ trạng thái minh bạch cho người dùng, không tạo ra cảm giác "lưu thành công ảo" nếu ESP32 đang không thể kết nối.

## 4. Kế hoạch Triển khai Code (Final Design)
* Tạo thư mục `lib/features/automation`.
* Khởi tạo `DeviceConfigModel` và `ScheduleModel` dựa vào CSDL.
* Khởi tạo `IAutomationRepository` và implementation hỗ trợ ném payload vào Queue nếu Exception kết nối xảy ra.
* Giao diện `AutomationPage` sẽ thay thế cho `SensorMonitorPage` hiện tại ở Tab 2 của Bottom Navigation.
