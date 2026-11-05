import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/sharing/domain/entities/device_share.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_bloc.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_event.dart';
import 'package:watering_app/features/sharing/presentation/bloc/sharing_state.dart';
import 'package:watering_app/features/sharing/presentation/pages/family_guests_page.dart';

class MockSharingBloc extends MockBloc<SharingEvent, SharingState>
    implements SharingBloc {}

void main() {
  late MockSharingBloc bloc;

  const noViewers = [
    DeviceShare(deviceId: 'A', deviceName: 'Vườn sau', viewers: []),
    DeviceShare(deviceId: 'B', deviceName: 'Ban công', viewers: []),
  ];
  const withViewer = [
    DeviceShare(
      deviceId: 'A',
      deviceName: 'Vườn sau',
      viewers: [SharedViewer(id: 7, email: 'a@test.com', isLocked: false)],
    ),
    DeviceShare(deviceId: 'B', deviceName: 'Ban công', viewers: []),
  ];

  setUpAll(() {
    registerFallbackValue(const SharingLoadRequested());
  });

  setUp(() {
    bloc = MockSharingBloc();
  });

  Future<void> pumpPage(WidgetTester tester, SharingState state) async {
    when(() => bloc.state).thenReturn(state);
    await tester.pumpWidget(MaterialApp(
      home: BlocProvider<SharingBloc>.value(
        value: bloc,
        child: const FamilyGuestsPage(),
      ),
    ));
  }

  testWidgets('explains that only owners can share', (tester) async {
    await pumpPage(tester, const SharingLoaded(shares: []));

    expect(find.text('Bạn chưa sở hữu thiết bị nào'), findsOneWidget);
    expect(find.byType(FloatingActionButton), findsNothing);
  });

  testWidgets('prompts to add someone when nothing is shared', (tester) async {
    await pumpPage(tester, const SharingLoaded(shares: noViewers));

    expect(find.text('Chưa chia sẻ với ai'), findsOneWidget);
    expect(find.byType(FloatingActionButton), findsOneWidget);
  });

  testWidgets('adds a person in two steps: email, then devices', (tester) async {
    await pumpPage(tester, const SharingLoaded(shares: withViewer));

    await tester.tap(find.byType(FloatingActionButton));
    await tester.pumpAndSettle();

    await tester.tap(find.text('Tiếp tục'));
    await tester.pump();
    expect(find.text('Vui lòng nhập email'), findsOneWidget);

    await tester.enterText(find.byType(TextFormField), ' a@test.com ');
    await tester.tap(find.text('Tiếp tục'));
    await tester.pump();

    expect(find.text('Chọn thiết bị chia sẻ với a@test.com'), findsOneWidget);
    expect(find.text('Đã chia sẻ'), findsOneWidget);
    final shareButton = find.widgetWithText(ElevatedButton, 'Thêm chia sẻ');
    expect(tester.widget<ElevatedButton>(shareButton).onPressed, isNull);

    await tester.tap(find.text('Ban công'));
    await tester.pump();
    await tester.tap(shareButton);
    await tester.pump();

    verify(() => bloc.add(const SharingViewerAdded(
          email: 'a@test.com',
          deviceIds: ['B'],
        ))).called(1);
  });

  testWidgets('locks a viewer with the switch', (tester) async {
    await pumpPage(tester, const SharingLoaded(shares: withViewer));

    expect(find.text('a@test.com'), findsOneWidget);
    expect(find.text('Được phép điều khiển'), findsOneWidget);

    await tester.tap(find.byType(Switch));
    await tester.pump();

    verify(() => bloc.add(const SharingViewerLockChanged(
          deviceId: 'A',
          viewerId: 7,
          locked: true,
        ))).called(1);
  });

  testWidgets('asks for confirmation before removing a viewer', (tester) async {
    await pumpPage(tester, const SharingLoaded(shares: withViewer));

    await tester.tap(find.byIcon(Icons.delete_outline));
    await tester.pumpAndSettle();
    verifyNever(() => bloc.add(any()));

    await tester.tap(find.widgetWithText(FilledButton, 'Ngừng chia sẻ'));
    await tester.pumpAndSettle();

    verify(() => bloc.add(const SharingViewerRemoved(
          deviceId: 'A',
          viewerId: 7,
        ))).called(1);
  });

  testWidgets('shows errors from the server', (tester) async {
    whenListen(
      bloc,
      Stream<SharingState>.fromIterable([
        const SharingLoaded(shares: withViewer, errorMessage: 'Không có quyền'),
      ]),
      initialState: const SharingLoaded(shares: withViewer),
    );
    await tester.pumpWidget(MaterialApp(
      home: BlocProvider<SharingBloc>.value(
        value: bloc,
        child: const FamilyGuestsPage(),
      ),
    ));
    await tester.pump();

    expect(find.text('Không có quyền'), findsOneWidget);
  });
}
