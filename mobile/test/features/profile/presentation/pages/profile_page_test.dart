import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/auth/domain/entities/user.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_state.dart';
import 'package:watering_app/features/profile/presentation/pages/profile_page.dart';

class MockAuthBloc extends MockBloc<AuthEvent, AuthState> implements AuthBloc {}

void main() {
  late MockAuthBloc mockAuthBloc;

  const testUser = User(
    id: 1,
    username: 'testuser',
    email: 'test@example.com',
    fullName: 'Test User',
    role: UserRole.user,
  );

  setUpAll(() {
    registerFallbackValue(const AuthLogoutRequested());
  });

  setUp(() {
    mockAuthBloc = MockAuthBloc();
  });

  Widget buildWidget() {
    return MaterialApp(
      home: BlocProvider<AuthBloc>.value(
        value: mockAuthBloc,
        child: const ProfilePage(),
      ),
    );
  }

  group('ProfilePage', () {
    testWidgets('shows unauthenticated message when not authenticated', (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthUnauthenticated());

      await tester.pumpWidget(buildWidget());

      expect(find.text('Chưa đăng nhập'), findsOneWidget);
    });

    testWidgets('shows user profile with "Người dùng" role badge and settings menu', (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthAuthenticated(user: testUser));

      await tester.pumpWidget(buildWidget());

      expect(find.text('Test User'), findsOneWidget);
      expect(find.text('test@example.com'), findsOneWidget);
      expect(find.text('Người dùng'), findsOneWidget);
      expect(find.text('Chủ thiết bị'), findsNothing);

      // Settings menu checks
      expect(find.text('Quản lý thiết bị'), findsOneWidget);
      expect(find.text('Gia đình và khách'), findsOneWidget);
      expect(find.text('Cài đặt thông báo'), findsNothing);
      expect(find.text('Về ứng dụng'), findsOneWidget);
      expect(find.text('Đăng xuất'), findsOneWidget);
    });

    testWidgets('tapping logout shows confirmation dialog and dispatches AuthLogoutRequested', (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthAuthenticated(user: testUser));

      await tester.pumpWidget(buildWidget());

      await tester.tap(find.text('Đăng xuất'));
      await tester.pumpAndSettle();

      expect(find.text('Bạn có chắc chắn muốn đăng xuất?'), findsOneWidget);

      await tester.tap(find.widgetWithText(FilledButton, 'Đăng xuất'));
      await tester.pumpAndSettle();

      verify(() => mockAuthBloc.add(const AuthLogoutRequested())).called(1);
    });
  });
}

