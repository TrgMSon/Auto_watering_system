/// Lớp kết nối BLE ở mức GATT. Bản thật sẽ dùng thư viện BLE khi firmware sẵn sàng;
/// trong lúc đó kiểm thử chạy với ESP32 giả ở tool/fake_esp32.dart.
abstract interface class BleTransport {
  /// Số byte tối đa của một lần ghi theo MTU đã đàm phán (MTU trừ 3).
  int get maxWritePayloadBytes;

  Future<List<int>> read(String characteristicUuid);

  /// Ghi có phản hồi. Phản hồi GATT chỉ xác nhận đã nhận, không phải lệnh đã thành công.
  /// Với lệnh bơm và giờ, firmware chỉ trả phản hồi khi đã xử lý xong lệnh.
  /// Ném `BleWriteRejectedException` khi ESP32 trả lỗi ATT.
  /// Phải xin MTU trước khi gửi lệnh: với MTU mặc định 23, firmware từ chối lệnh
  /// vì gói kết quả không vừa.
  Future<void> write(String characteristicUuid, List<int> value);

  /// Bật Notify và trả về luồng dữ liệu; luồng đóng khi mất kết nối.
  Future<Stream<List<int>>> subscribe(String characteristicUuid);
}
