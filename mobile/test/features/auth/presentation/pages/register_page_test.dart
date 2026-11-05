import 'package:bloc_test/bloc_test.dart';
import 'package:flutter/material.dart';
import 'package:flutter_bloc/flutter_bloc.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:go_router/go_router.dart';
import 'package:mocktail/mocktail.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_bloc.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_event.dart';
import 'package:watering_app/features/auth/presentation/bloc/register_state.dart';
import 'package:watering_app/features/auth/presentation/pages/register_page.dart';

class MockRegisterBloc extends MockBloc<RegisterEvent, RegisterState>
    implements RegisterBloc {}

void main() {
  late MockRegisterBloc mockRegisterBloc;

  setUpAll(() {
    registerFallbackValue(const RegisterSubmitted(email: '', password: ''));
  });

  setUp(() {
    mockRegisterBloc = MockRegisterBloc();
  });

  Widget createWidget() {
    return MaterialApp(
      home: BlocProvider<RegisterBloc>.value(
        value: mockRegisterBloc,
        child: const RegisterPage(),
      ),
    );
  }

  Future<void> fillForm(
    WidgetTester tester, {
    required String password,
    required String confirmation,
  }) async {
    await tester.enterText(
      find.widgetWithText(TextFormField, 'Email'),
      ' new@test.com ',
    );
    await tester.enterText(
      find.widgetWithText(TextFormField, 'Mật khẩu'),
      password,
    );
    await tester.enterText(
      find.widgetWithText(TextFormField, 'Nhập lại mật khẩu'),
      confirmation,
    );
    await tester.tap(find.widgetWithText(ElevatedButton, 'Đăng ký'));
    await tester.pump();
  }

  group('RegisterPage', () {
    testWidgets('submits the trimmed email and password', (tester) async {
      when(() => mockRegisterBloc.state).thenReturn(const RegisterInitial());

      await tester.pumpWidget(createWidget());
      await fillForm(tester, password: 'password123', confirmation: 'password123');

      verify(() => mockRegisterBloc.add(const RegisterSubmitted(
            email: 'new@test.com',
            password: 'password123',
          ))).called(1);
    });

    testWidgets('rejects a password shorter than the backend allows',
        (tester) async {
      when(() => mockRegisterBloc.state).thenReturn(const RegisterInitial());

      await tester.pumpWidget(createWidget());
      await fillForm(tester, password: '1234567', confirmation: '1234567');

      expect(find.text('Mật khẩu phải có ít nhất 8 ký tự'), findsOneWidget);
      verifyNever(() => mockRegisterBloc.add(any()));
    });

    testWidgets('rejects a confirmation that does not match', (tester) async {
      when(() => mockRegisterBloc.state).thenReturn(const RegisterInitial());

      await tester.pumpWidget(createWidget());
      await fillForm(tester, password: 'password123', confirmation: 'password124');

      expect(find.text('Mật khẩu nhập lại không khớp'), findsOneWidget);
      verifyNever(() => mockRegisterBloc.add(any()));
    });

    testWidgets('shows the failure message', (tester) async {
      whenListen(
        mockRegisterBloc,
        Stream.fromIterable([
          const RegisterFailure(message: 'Email này đã được đăng ký trong hệ thống.'),
        ]),
        initialState: const RegisterInitial(),
      );

      await tester.pumpWidget(createWidget());
      await tester.pump();

      expect(find.text('Email này đã được đăng ký trong hệ thống.'), findsOneWidget);
    });

    testWidgets('returns the email to the previous page on success',
        (tester) async {
      whenListen(
        mockRegisterBloc,
        Stream.fromIterable([const RegisterSuccess(email: 'new@test.com')]),
        initialState: const RegisterInitial(),
      );
      String? returnedEmail;
      final router = GoRouter(
        routes: [
          GoRoute(
            path: '/',
            builder: (context, state) => TextButton(
              onPressed: () async {
                returnedEmail = await context.push<String>('/register');
              },
              child: const Text('Mở trang đăng ký'),
            ),
          ),
          GoRoute(
            path: '/register',
            builder: (context, state) => BlocProvider<RegisterBloc>.value(
              value: mockRegisterBloc,
              child: const RegisterPage(),
            ),
          ),
        ],
      );
      addTearDown(router.dispose);

      await tester.pumpWidget(MaterialApp.router(routerConfig: router));
      await tester.tap(find.text('Mở trang đăng ký'));
      await tester.pumpAndSettle();

      expect(returnedEmail, 'new@test.com');
    });
  });
}
