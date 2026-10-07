# Quyết định Kiến trúc Giao diện (UI Architecture Decision)
*Ngày tạo: 2026-10-06*

## 1. Tóm tắt Yêu cầu (Understanding Summary)
* **Mục tiêu:** Chuyển đổi 8 Use Cases hệ thống thành luồng điều hướng Mobile UI tối ưu cho ứng dụng IoT Smart Watering.
* **Đối tượng người dùng (Target Users):** Chỉ phục vụ Chủ sở hữu (Owner) và Người dùng chia sẻ (Viewer). Bỏ qua Admin (chức năng Admin sẽ ở Web).
* **Vấn đề cần giải quyết:** Loại bỏ thiết kế 1-1 theo Use Case (Task-Based) đang gây phân mảnh trải nghiệm. Chuyển sang giao diện Hướng Thiết Bị (Object-Centric).
* **Ràng buộc:** Dữ liệu hiện tại đang là Mock Data, chưa tích hợp Backend thực. UI cần xử lý việc ẩn/hiện chức năng tùy theo Role (Owner/Viewer).

## 2. Các Giả định & Rủi ro (Assumptions & Risks)
* **Giả định:** Ứng dụng dùng `go_router` với `StatefulShellRoute` để duy trì trạng thái của Bottom Navigation Bar.
* **Rủi ro:** Các lỗi hiển thị hiện tại (lỗi render biến chuỗi `String Interpolation`, lỗi thiếu font icon Material/Cupertino) cần được fix dứt điểm. Việc ẩn tab/menu dựa trên Role yêu cầu quản lý State chặt chẽ qua BLoC.

## 3. Nhật ký Quyết định (Decision Log)
1. **[Quyết định] Không thiết kế UI rời rạc theo Use Case.**
   * *Lý do:* Trải nghiệm UX tồi, bắt người dùng điều hướng quá nhiều.
2. **[Quyết định] Sử dụng Kiến trúc Object-Centric Dashboard (4 Tabs).**
   * *Lý do:* Gộp các Use Case vào luồng thao tác tự nhiên. 
   * *Thiết kế chốt:* 
     - **Tab 1 (Tổng quan):** Cảm biến (`View stats`) + Bật/tắt bơm thủ công (`Manage watering`).
     - **Tab 2 (Tự động hóa):** Cài đặt ngưỡng & Hẹn giờ tưới (`Manage watering`).
     - **Tab 3 (Thành viên):** Quản lý Viewer (`Manage viewers`) - Ẩn đối với Role Viewer.
     - **Tab 4 (Cài đặt/Hồ sơ):** Thông tin cá nhân (`View profile`) & Đăng xuất.
3. **[Quyết định] Fix trực tiếp UI hiện tại của Profile Screen.**
   * *Hành động:* Đổi huy hiệu "Quản trị viên" thành "Chủ thiết bị". Fix lỗi chuỗi avatar/email. Đưa mục "Quản lý người dùng" thành Tab độc lập hoặc ẩn đi nếu không đủ quyền.

## 4. Final Design
* Thiết kế được chốt sẽ tiến hành lập trình thay đổi cấu trúc `AppRouter`, điều chỉnh BottomNavigationBar trong `ScaffoldWithNavBar`, và refactor lại file `profile_page.dart`.
