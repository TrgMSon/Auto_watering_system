import 'package:intl/intl.dart';

class Formatters {
  Formatters._();

  static String formatDateTime(DateTime dt) {
    return DateFormat('dd/MM/yyyy HH:mm').format(dt);
  }

  static String formatTime(DateTime dt) {
    return DateFormat('HH:mm:ss').format(dt);
  }

  static String formatRelativeTime(DateTime dt) {
    final now = DateTime.now();
    final diff = now.difference(dt);
    if (diff.inMinutes < 1) return 'Vừa xong';
    if (diff.inMinutes < 60) return '${diff.inMinutes} phút trước';
    if (diff.inHours < 24) return '${diff.inHours} giờ trước';
    return formatDateTime(dt);
  }

  static String formatSensorValue(double value, {int decimals = 1}) {
    return value.toStringAsFixed(decimals);
  }
}
