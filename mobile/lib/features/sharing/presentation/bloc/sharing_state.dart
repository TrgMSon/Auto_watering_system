import 'package:equatable/equatable.dart';
import '../../domain/entities/device_share.dart';

sealed class SharingState extends Equatable {
  const SharingState();

  @override
  List<Object?> get props => [];
}

class SharingInitial extends SharingState {
  const SharingInitial();
}

class SharingLoading extends SharingState {
  const SharingLoading();
}

class SharingLoaded extends SharingState {
  final List<DeviceShare> shares;
  final bool isSaving;
  final String? errorMessage;

  /// Chỉ đúng ở trạng thái ngay sau khi đã chia sẻ thành công mọi thiết bị đã chọn.
  final bool shareCompleted;

  const SharingLoaded({
    required this.shares,
    this.isSaving = false,
    this.errorMessage,
    this.shareCompleted = false,
  });

  SharingLoaded savingCopy() => SharingLoaded(shares: shares, isSaving: true);

  @override
  List<Object?> get props => [shares, isSaving, errorMessage, shareCompleted];
}

class SharingError extends SharingState {
  final String message;

  const SharingError({required this.message});

  @override
  List<Object?> get props => [message];
}
