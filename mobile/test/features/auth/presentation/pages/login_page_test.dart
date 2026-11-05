import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:go_router/go_router.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/app/routes/route_names.dart';
import 'package:watering_app/core/widgets/app_loading_indicator.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/auth_state.dart';
import 'package:watering_app/features/auth/presentation/pages/login_page.dart';

class MockAuthBloc extends MockBloc<AuthEvent, AuthState> implements AuthBloc {}

void main() {
  late MockAuthBloc mockAuthBloc;

  setUpAll(() {
    registerFallbackValue(const AuthCheckRequested());
    registerFallbackValue(const AuthLoginRequested(username: '', password: ''));
  });

  setUp(() {
    mockAuthBloc = MockAuthBloc();
  });

  Widget createWidget() {
    return MaterialApp(
      home: BlocProvider<AuthBloc>.value(
        value: mockAuthBloc,
        child: const LoginPage(),
      ),
    );
  }

  group('LoginPage', () {
    testWidgets(
        'shows loading indicator and no login form when state is AuthInitial or AuthChecking',
        (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthChecking());

      await tester.pumpWidget(createWidget());

      expect(find.byType(AppLoadingIndicator), findsOneWidget);
      expect(find.text('Đang kiểm tra phiên đăng nhập...'), findsOneWidget);
      expect(find.text('Email'), findsNothing);
      expect(find.text('Mật khẩu'), findsNothing);
    });

    testWidgets('shows login form when state is AuthUnauthenticated',
        (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthUnauthenticated());

      await tester.pumpWidget(createWidget());

      expect(find.text('Email'), findsOneWidget);
      expect(find.text('Mật khẩu'), findsOneWidget);
      expect(find.widgetWithText(ElevatedButton, 'Đăng nhập'), findsOneWidget);
    });

    testWidgets('shows validation error when email is empty or invalid',
        (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthUnauthenticated());

      await tester.pumpWidget(createWidget());

      await tester.tap(find.widgetWithText(ElevatedButton, 'Đăng nhập'));
      await tester.pump();

      expect(find.text('Vui lòng nhập email'), findsOneWidget);

      await tester.enterText(
        find.widgetWithText(TextFormField, 'Email'),
        'invalid-email',
      );
      await tester.tap(find.widgetWithText(ElevatedButton, 'Đăng nhập'));
      await tester.pump();

      expect(find.text('Vui lòng nhập email hợp lệ'), findsOneWidget);
    });

    testWidgets('submitting valid form dispatches AuthLoginRequested',
        (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthUnauthenticated());

      await tester.pumpWidget(createWidget());

      await tester.enterText(
        find.widgetWithText(TextFormField, 'Email'),
        'user@test.com',
      );
      await tester.enterText(
        find.widgetWithText(TextFormField, 'Mật khẩu'),
        '123456',
      );
      await tester.tap(find.widgetWithText(ElevatedButton, 'Đăng nhập'));
      await tester.pump();

      verify(() => mockAuthBloc.add(const AuthLoginRequested(
            username: 'user@test.com',
            password: '123456',
          ))).called(1);
    });

    testWidgets('fills in the email after registering from the login form',
        (tester) async {
      when(() => mockAuthBloc.state).thenReturn(const AuthUnauthenticated());
      final router = GoRouter(
        routes: [
          GoRoute(
            path: '/',
            builder: (context, state) => BlocProvider<AuthBloc>.value(
              value: mockAuthBloc,
              child: const LoginPage(),
            ),
          ),
          GoRoute(
            path: RouteNames.register,
            builder: (context, state) => TextButton(
              onPressed: () => context.pop('new@test.com'),
              child: const Text('Hoàn tất đăng ký'),
            ),
          ),
        ],
      );
      addTearDown(router.dispose);

      await tester.pumpWidget(MaterialApp.router(routerConfig: router));
      await tester.tap(find.text('Chưa có tài khoản? Đăng ký'));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Hoàn tất đăng ký'));
      await tester.pumpAndSettle();

      expect(find.text('new@test.com'), findsOneWidget);
      expect(find.text('Đăng ký thành công. Vui lòng đăng nhập.'), findsOneWidget);
    });

    testWidgets('shows SnackBar on AuthError state', (tester) async {
      whenListen(
        mockAuthBloc,
        Stream.fromIterable([
          const AuthError(message: 'Tài khoản không tồn tại'),
        ]),
        initialState: const AuthUnauthenticated(),
      );

      await tester.pumpWidget(createWidget());
      await tester.pump(); // Process stream event and trigger SnackBar animation

      expect(find.text('Tài khoản không tồn tại'), findsOneWidget);
    });
  });
}

