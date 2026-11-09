import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../ble/presentation/bloc/ble_session_cubit.dart';
import '../../domain/entities/device.dart';

typedef WifiCredentials = ({String ssid, String password});

/// Mở form nhập Wi-Fi rồi gửi tới ESP32 qua Bluetooth (`0006`).
/// Thông tin Wi-Fi không được lưu trên điện thoại hay server.
Future<void> showWifiConfigSheet(BuildContext context, Device device) async {
  final credentials = await showModalBottomSheet<WifiCredentials>(
    context: context,
    isScrollControlled: true,
    useSafeArea: true,
    builder: (_) => WifiConfigSheet(device: device),
  );
  if (credentials == null || !context.mounted) return;

  final messenger = ScaffoldMessenger.of(context);
  messenger.showSnackBar(const SnackBar(
    content: Text('Đang gửi Wi-Fi tới thiết bị, có thể mất đến 35 giây...'),
    duration: Duration(seconds: 40),
  ));
  final error = await context.read<BleSessionCubit>().sendWifi(
        device.id,
        ssid: credentials.ssid,
        password: credentials.password,
      );
  messenger.hideCurrentSnackBar();
  messenger.showSnackBar(SnackBar(
    content: Text(error ?? 'Thiết bị đã kết nối Wi-Fi.'),
  ));
}

class WifiConfigSheet extends StatefulWidget {
  final Device device;
  const WifiConfigSheet({super.key, required this.device});

  @override
  State<WifiConfigSheet> createState() => _WifiConfigSheetState();
}

class _WifiConfigSheetState extends State<WifiConfigSheet> {
  // Giới hạn theo firmware: SSID tối đa 32 byte, mật khẩu WPA2 từ 8 đến 63 byte, không có dấu `|`.
  static const int _maxSsidBytes = 32;
  static const int _minPasswordLength = 8;
  static const int _maxPasswordLength = 63;

  final _formKey = GlobalKey<FormState>();
  final _ssidController = TextEditingController();
  final _passwordController = TextEditingController();
  bool _obscurePassword = true;

  @override
  void dispose() {
    _ssidController.dispose();
    _passwordController.dispose();
    super.dispose();
  }

  void _onSubmit() {
    if (_formKey.currentState?.validate() ?? false) {
      Navigator.of(context).pop<WifiCredentials>(
        (ssid: _ssidController.text, password: _passwordController.text),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        left: 24,
        right: 24,
        top: 24,
        bottom: MediaQuery.of(context).viewInsets.bottom + 24,
      ),
      child: SingleChildScrollView(
        child: Form(
          key: _formKey,
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Text(
                'Cấu hình Wi-Fi',
                style: Theme.of(context).textTheme.titleLarge,
              ),
              const SizedBox(height: 4),
              Text(
                widget.device.name,
                style: Theme.of(context).textTheme.bodyMedium,
              ),
              const SizedBox(height: 24),
              _buildSsidField(),
              const SizedBox(height: 16),
              _buildPasswordField(),
              const SizedBox(height: 24),
              SizedBox(
                height: 52,
                child: ElevatedButton(
                  onPressed: _onSubmit,
                  child: const Text('Gửi tới thiết bị'),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildSsidField() {
    return TextFormField(
      controller: _ssidController,
      textInputAction: TextInputAction.next,
      autocorrect: false,
      enableSuggestions: false,
      decoration: InputDecoration(
        labelText: 'Tên Wi-Fi',
        prefixIcon: const Icon(Icons.wifi),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
        ),
      ),
      validator: (value) {
        if (value == null || value.trim().isEmpty) {
          return 'Vui lòng nhập tên Wi-Fi';
        }
        if (utf8.encode(value).length > _maxSsidBytes) {
          return 'Tên Wi-Fi quá dài';
        }
        if (value.contains('|')) {
          return 'Tên Wi-Fi không được chứa dấu |';
        }
        return null;
      },
    );
  }

  Widget _buildPasswordField() {
    return TextFormField(
      controller: _passwordController,
      obscureText: _obscurePassword,
      autocorrect: false,
      enableSuggestions: false,
      decoration: InputDecoration(
        labelText: 'Mật khẩu Wi-Fi',
        prefixIcon: const Icon(Icons.lock_outline),
        suffixIcon: IconButton(
          icon: Icon(
            _obscurePassword ? Icons.visibility_off : Icons.visibility,
          ),
          onPressed: () {
            setState(() => _obscurePassword = !_obscurePassword);
          },
        ),
        border: OutlineInputBorder(
          borderRadius: BorderRadius.circular(12),
        ),
      ),
      validator: (value) {
        if (value == null || value.isEmpty) {
          return 'Vui lòng nhập mật khẩu Wi-Fi';
        }
        final length = utf8.encode(value).length;
        if (length < _minPasswordLength || length > _maxPasswordLength) {
          return 'Mật khẩu phải có từ $_minPasswordLength đến $_maxPasswordLength ký tự';
        }
        if (value.contains('|')) {
          return 'Mật khẩu không được chứa dấu |';
        }
        return null;
      },
      onFieldSubmitted: (_) => _onSubmit(),
    );
  }
}
