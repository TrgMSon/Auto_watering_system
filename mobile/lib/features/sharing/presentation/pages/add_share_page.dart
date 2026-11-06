import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../domain/entities/device_share.dart';
import '../bloc/sharing_bloc.dart';
import '../bloc/sharing_event.dart';
import '../bloc/sharing_state.dart';

/// Bước 1 nhập email người nhận, bước 2 chọn thiết bị muốn chia sẻ.
class AddSharePage extends StatefulWidget {
  const AddSharePage({super.key});

  @override
  State<AddSharePage> createState() => _AddSharePageState();
}

class _AddSharePageState extends State<AddSharePage> {
  final _formKey = GlobalKey<FormState>();
  final _emailController = TextEditingController();
  final Set<String> _selectedDeviceIds = {};
  bool _isChoosingDevices = false;

  String get _email => _emailController.text.trim();

  @override
  void dispose() {
    _emailController.dispose();
    super.dispose();
  }

  void _onContinue() {
    if (_formKey.currentState?.validate() ?? false) {
      setState(() => _isChoosingDevices = true);
    }
  }

  void _onShare() {
    context.read<SharingBloc>().add(
          SharingViewerAdded(
            email: _email,
            deviceIds: _selectedDeviceIds.toList(),
          ),
        );
  }

  @override
  Widget build(BuildContext context) {
    return BlocConsumer<SharingBloc, SharingState>(
      listener: (context, state) {
        if (state is! SharingLoaded) return;
        if (state.shareCompleted) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Đã chia sẻ thiết bị với $_email.')),
          );
          Navigator.of(context).pop();
        } else if (state.errorMessage != null) {
          // Trang danh sách phía dưới cũng nghe lỗi này và hiện SnackBar.
          setState(() {
            _selectedDeviceIds.removeWhere(
              (id) => state.shares.any(
                (share) => share.deviceId == id && share.isSharedWith(_email),
              ),
            );
          });
        }
      },
      builder: (context, state) {
        return Scaffold(
          appBar: AppBar(title: const Text('Thêm người dùng')),
          body: SafeArea(
            child: _isChoosingDevices && state is SharingLoaded
                ? _buildDeviceStep(state)
                : _buildEmailStep(),
          ),
        );
      },
    );
  }

  Widget _buildEmailStep() {
    return SingleChildScrollView(
      padding: const EdgeInsets.all(24),
      child: Form(
        key: _formKey,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(
              'Nhập email người nhận đã dùng để đăng ký tài khoản.',
              style: Theme.of(context).textTheme.bodyMedium,
            ),
            const SizedBox(height: 16),
            TextFormField(
              controller: _emailController,
              keyboardType: TextInputType.emailAddress,
              decoration: InputDecoration(
                labelText: 'Email',
                prefixIcon: const Icon(Icons.email_outlined),
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(12),
                ),
              ),
              validator: (value) {
                if (value == null || value.trim().isEmpty) {
                  return 'Vui lòng nhập email';
                }
                if (!RegExp(r'^[^@]+@[^@]+\.[^@]+$').hasMatch(value.trim())) {
                  return 'Vui lòng nhập email hợp lệ';
                }
                return null;
              },
              onFieldSubmitted: (_) => _onContinue(),
            ),
            const SizedBox(height: 24),
            SizedBox(
              height: 52,
              child: ElevatedButton(
                onPressed: _onContinue,
                child: const Text('Tiếp tục'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildDeviceStep(SharingLoaded state) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Expanded(
          child: ListView(
            padding: const EdgeInsets.symmetric(vertical: 16),
            children: [
              Padding(
                padding: const EdgeInsets.symmetric(horizontal: 24),
                child: Text(
                  'Chọn thiết bị chia sẻ với $_email',
                  style: Theme.of(context).textTheme.titleMedium,
                ),
              ),
              const SizedBox(height: 8),
              for (final share in state.shares) _buildDeviceTile(share, state.isSaving),
              const Padding(
                padding: EdgeInsets.all(24),
                child: Text(
                  'Người nhận có thể xem và điều khiển thiết bị, nhưng không thể xóa '
                  'lịch tưới hay chia sẻ tiếp. Quyền có hiệu lực ngay, không cần chấp nhận.',
                  style: TextStyle(color: AppColors.textSecondary),
                ),
              ),
            ],
          ),
        ),
        Padding(
          padding: const EdgeInsets.all(24),
          child: state.isSaving
              ? const AppLoadingIndicator(message: 'Đang chia sẻ...')
              : SizedBox(
                  height: 52,
                  child: ElevatedButton(
                    onPressed: _selectedDeviceIds.isEmpty ? null : _onShare,
                    child: const Text('Thêm chia sẻ'),
                  ),
                ),
        ),
      ],
    );
  }

  Widget _buildDeviceTile(DeviceShare share, bool isSaving) {
    final alreadyShared = share.isSharedWith(_email);
    return CheckboxListTile(
      title: Text(share.deviceName),
      subtitle: alreadyShared ? const Text('Đã chia sẻ') : null,
      value: alreadyShared || _selectedDeviceIds.contains(share.deviceId),
      onChanged: alreadyShared || isSaving
          ? null
          : (selected) {
              setState(() {
                if (selected ?? false) {
                  _selectedDeviceIds.add(share.deviceId);
                } else {
                  _selectedDeviceIds.remove(share.deviceId);
                }
              });
            },
    );
  }
}
