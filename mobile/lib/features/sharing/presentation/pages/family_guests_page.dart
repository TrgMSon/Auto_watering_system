import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import '../../../../core/theme/app_colors.dart';
import '../../../../core/widgets/app_error_widget.dart';
import '../../../../core/widgets/app_loading_indicator.dart';
import '../../domain/entities/device_share.dart';
import '../bloc/sharing_bloc.dart';
import '../bloc/sharing_event.dart';
import '../bloc/sharing_state.dart';
import 'add_share_page.dart';

class FamilyGuestsPage extends StatelessWidget {
  const FamilyGuestsPage({super.key});

  void _openAddShare(BuildContext context) {
    final bloc = context.read<SharingBloc>();
    Navigator.of(context).push(
      MaterialPageRoute<void>(
        builder: (_) => BlocProvider.value(
          value: bloc,
          child: const AddSharePage(),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return BlocConsumer<SharingBloc, SharingState>(
      listener: (context, state) {
        if (state is SharingLoaded && state.errorMessage != null) {
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
              content: Text(state.errorMessage!),
              backgroundColor: AppColors.error,
            ),
          );
        }
      },
      builder: (context, state) {
        final canAdd = state is SharingLoaded && state.shares.isNotEmpty;
        return Scaffold(
          appBar: AppBar(title: const Text('Gia đình và khách')),
          floatingActionButton: canAdd
              ? FloatingActionButton(
                  tooltip: 'Thêm người dùng',
                  onPressed: () => _openAddShare(context),
                  child: const Icon(Icons.add),
                )
              : null,
          body: _buildBody(context, state),
        );
      },
    );
  }

  Widget _buildBody(BuildContext context, SharingState state) {
    if (state is SharingError) {
      return AppErrorWidget(
        message: state.message,
        onRetry: () =>
            context.read<SharingBloc>().add(const SharingLoadRequested()),
      );
    }
    if (state is! SharingLoaded) {
      return const AppLoadingIndicator(message: 'Đang tải danh sách chia sẻ...');
    }
    if (state.shares.isEmpty) {
      return const _Message(
        title: 'Bạn chưa sở hữu thiết bị nào',
        subtitle: 'Chỉ chủ sở hữu mới có thể chia sẻ thiết bị.',
      );
    }

    final people = _groupByPerson(state.shares);
    if (people.isEmpty) {
      return const _Message(
        title: 'Chưa chia sẻ với ai',
        subtitle: 'Nhấn dấu cộng để thêm người dùng bằng email đã đăng ký.',
      );
    }
    return ListView(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 88),
      children: [
        for (final entry in people.entries)
          _PersonCard(
            email: entry.key,
            items: entry.value,
            isSaving: state.isSaving,
          ),
      ],
    );
  }

  Map<String, List<_SharedDevice>> _groupByPerson(List<DeviceShare> shares) {
    final people = <String, List<_SharedDevice>>{};
    for (final share in shares) {
      for (final viewer in share.viewers) {
        people.putIfAbsent(viewer.email, () => []).add(_SharedDevice(share, viewer));
      }
    }
    return people;
  }
}

class _SharedDevice {
  final DeviceShare share;
  final SharedViewer viewer;

  const _SharedDevice(this.share, this.viewer);
}

class _Message extends StatelessWidget {
  final String title;
  final String subtitle;

  const _Message({required this.title, required this.subtitle});

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.group_outlined, size: 64, color: AppColors.textSecondary),
            const SizedBox(height: 16),
            Text(
              title,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 8),
            Text(
              subtitle,
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium,
            ),
          ],
        ),
      ),
    );
  }
}

class _PersonCard extends StatelessWidget {
  final String email;
  final List<_SharedDevice> items;
  final bool isSaving;

  const _PersonCard({
    required this.email,
    required this.items,
    required this.isSaving,
  });

  Future<void> _confirmRemove(BuildContext context, _SharedDevice item) async {
    final bloc = context.read<SharingBloc>();
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (dialogContext) => AlertDialog(
        title: const Text('Ngừng chia sẻ'),
        content: Text(
          '$email sẽ không còn truy cập ${item.share.deviceName}.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(dialogContext).pop(false),
            child: const Text('Hủy'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(dialogContext).pop(true),
            child: const Text('Ngừng chia sẻ'),
          ),
        ],
      ),
    );
    if (confirmed == true) {
      bloc.add(SharingViewerRemoved(
        deviceId: item.share.deviceId,
        viewerId: item.viewer.id,
      ));
    }
  }

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          ListTile(
            leading: const Icon(Icons.person_outline, color: AppColors.primary),
            title: Text(email),
          ),
          const Divider(height: 1),
          for (final item in items)
            ListTile(
              title: Text(item.share.deviceName),
              subtitle: Text(item.viewer.isLocked ? 'Đang tạm khóa' : 'Được phép điều khiển'),
              trailing: Row(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Switch(
                    value: !item.viewer.isLocked,
                    onChanged: isSaving
                        ? null
                        : (allowed) => context.read<SharingBloc>().add(
                              SharingViewerLockChanged(
                                deviceId: item.share.deviceId,
                                viewerId: item.viewer.id,
                                locked: !allowed,
                              ),
                            ),
                  ),
                  IconButton(
                    tooltip: 'Ngừng chia sẻ',
                    icon: const Icon(Icons.delete_outline),
                    onPressed: isSaving ? null : () => _confirmRemove(context, item),
                  ),
                ],
              ),
            ),
        ],
      ),
    );
  }
}
