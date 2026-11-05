import 'package:bloc_test/bloc_test.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/core/network/api_client.dart';
import 'package:watering_app/features/sharing/domain/entities/device_share.dart';
import 'package:watering_app/features/sharing/domain/repositories/sharing_repository.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_bloc.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_event.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_state.dart';

class MockSharingRepository extends Mock implements SharingRepository {}

void main() {
  late MockSharingRepository repository;

  const empty = [DeviceShare(deviceId: 'A', deviceName: 'Vườn', viewers: [])];
  const shared = [
    DeviceShare(
      deviceId: 'A',
      deviceName: 'Vườn',
      viewers: [SharedViewer(id: 7, email: 'a@test.com', isLocked: false)],
    ),
  ];

  setUp(() {
    repository = MockSharingRepository();
  });

  blocTest<SharingBloc, SharingState>(
    'loads the shares',
    build: () {
      when(repository.getOwnedDeviceShares).thenAnswer((_) async => shared);
      return SharingBloc(repository: repository);
    },
    act: (bloc) => bloc.add(const SharingLoadRequested()),
    expect: () => [const SharingLoading(), const SharingLoaded(shares: shared)],
  );

  blocTest<SharingBloc, SharingState>(
    'shows the error when loading fails',
    build: () {
      when(repository.getOwnedDeviceShares)
          .thenThrow(const ApiException(0, 'Mất kết nối'));
      return SharingBloc(repository: repository);
    },
    act: (bloc) => bloc.add(const SharingLoadRequested()),
    expect: () => [const SharingLoading(), const SharingError(message: 'Mất kết nối')],
  );

  blocTest<SharingBloc, SharingState>(
    'completes after sharing every selected device',
    build: () {
      when(() => repository.addViewer('A', 'a@test.com')).thenAnswer((_) async {});
      when(repository.getOwnedDeviceShares).thenAnswer((_) async => shared);
      return SharingBloc(repository: repository);
    },
    seed: () => const SharingLoaded(shares: empty),
    act: (bloc) =>
        bloc.add(const SharingViewerAdded(email: 'a@test.com', deviceIds: ['A'])),
    expect: () => [
      const SharingLoaded(shares: empty, isSaving: true),
      const SharingLoaded(shares: shared, shareCompleted: true),
    ],
  );

  blocTest<SharingBloc, SharingState>(
    'reports a partial failure and does not complete',
    build: () {
      when(() => repository.addViewer('A', 'a@test.com')).thenAnswer((_) async {});
      when(() => repository.addViewer('B', 'a@test.com'))
          .thenThrow(const ApiException(404, 'Không tìm thấy tài khoản với email này'));
      when(repository.getOwnedDeviceShares).thenAnswer((_) async => shared);
      return SharingBloc(repository: repository);
    },
    seed: () => const SharingLoaded(shares: empty),
    act: (bloc) => bloc.add(
      const SharingViewerAdded(email: 'a@test.com', deviceIds: ['A', 'B']),
    ),
    expect: () => [
      const SharingLoaded(shares: empty, isSaving: true),
      const SharingLoaded(
        shares: shared,
        errorMessage: 'Đã chia sẻ 1/2 thiết bị. Không tìm thấy tài khoản với email này',
      ),
    ],
  );

  blocTest<SharingBloc, SharingState>(
    'removes a viewer and reloads',
    build: () {
      when(() => repository.removeViewer('A', 7)).thenAnswer((_) async {});
      when(repository.getOwnedDeviceShares).thenAnswer((_) async => empty);
      return SharingBloc(repository: repository);
    },
    seed: () => const SharingLoaded(shares: shared),
    act: (bloc) => bloc.add(const SharingViewerRemoved(deviceId: 'A', viewerId: 7)),
    expect: () => [
      const SharingLoaded(shares: shared, isSaving: true),
      const SharingLoaded(shares: empty),
    ],
  );

  blocTest<SharingBloc, SharingState>(
    'keeps the list and shows the error when locking fails',
    build: () {
      when(() => repository.setViewerLocked('A', 7, locked: true))
          .thenThrow(const ApiException(409, 'Không có quyền'));
      when(repository.getOwnedDeviceShares).thenAnswer((_) async => shared);
      return SharingBloc(repository: repository);
    },
    seed: () => const SharingLoaded(shares: shared),
    act: (bloc) => bloc.add(
      const SharingViewerLockChanged(deviceId: 'A', viewerId: 7, locked: true),
    ),
    expect: () => [
      const SharingLoaded(shares: shared, isSaving: true),
      const SharingLoaded(shares: shared, errorMessage: 'Không có quyền'),
    ],
  );
}
