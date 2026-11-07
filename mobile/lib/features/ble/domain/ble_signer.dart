import 'dart:convert';

import 'package:crypto/crypto.dart';

/// Tạo chữ ký `sig`: HMAC-SHA256 lấy 16 ký tự hex thường đầu tiên.
class BleSigner {
  BleSigner._();

  static const int _signatureLength = 16;

  /// [secretKey] là nguyên chuỗi `ble_secret_key`, không giải mã Base64URL.
  static String sign(String secretKey, String message) {
    final digest = Hmac(sha256, utf8.encode(secretKey)).convert(utf8.encode(message));
    return digest.toString().substring(0, _signatureLength);
  }
}
