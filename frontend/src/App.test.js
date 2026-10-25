import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import App from './App';

const originalFetch = global.fetch;

beforeEach(() => {
  localStorage.clear();
  window.history.replaceState({}, '', '/');
});

afterEach(() => {
  global.fetch = originalFetch;
});

test('shows the administrator login page when no session exists', () => {
  render(<App />);
  expect(screen.getByRole('heading', { name: /chào mừng trở lại/i })).toBeInTheDocument();
  expect(screen.getByLabelText(/địa chỉ email/i)).toBeInTheDocument();
  expect(screen.getByLabelText('Mật khẩu')).toBeInTheDocument();
});

test('logs in an administrator and opens the dashboard', async () => {
  global.fetch = jest.fn().mockResolvedValue({
    ok: true,
    status: 200,
    json: async () => ({
      success: true,
      data: {
        accessToken: 'admin-token',
        refreshToken: 'refresh-token',
        role: 'ADMIN',
        userResponse: { email: 'admin@example.com', fullName: 'Garden Admin' },
      },
    }),
  });

  render(<App />);
  fireEvent.change(screen.getByLabelText(/địa chỉ email/i), {
    target: { value: 'admin@example.com' },
  });
  fireEvent.change(screen.getByLabelText('Mật khẩu'), {
    target: { value: 'admin-password' },
  });
  fireEvent.click(screen.getByRole('button', { name: /đăng nhập/i }));

  await waitFor(() => {
    expect(screen.getByRole('heading', { name: /tổng quan/i })).toBeInTheDocument();
  });
  expect(global.fetch).toHaveBeenCalledWith(
    '/api/auth/login',
    expect.objectContaining({
      method: 'POST',
      body: JSON.stringify({ email: 'admin@example.com', password: 'admin-password' }),
    }),
  );
  expect(localStorage.getItem('garden-admin-auth')).toContain('admin-token');
});

test('does not grant dashboard access to a non-admin account', async () => {
  global.fetch = jest.fn().mockResolvedValue({
    ok: true,
    status: 200,
    json: async () => ({
      success: true,
      data: {
        accessToken: 'user-token',
        refreshToken: 'refresh-token',
        role: 'USER',
        userResponse: { email: 'user@example.com' },
      },
    }),
  });

  render(<App />);
  fireEvent.change(screen.getByLabelText(/địa chỉ email/i), {
    target: { value: 'user@example.com' },
  });
  fireEvent.change(screen.getByLabelText('Mật khẩu'), {
    target: { value: 'user-password' },
  });
  fireEvent.click(screen.getByRole('button', { name: /đăng nhập/i }));

  expect(await screen.findByRole('alert')).toHaveTextContent(/không có quyền quản trị viên/i);
  expect(screen.queryByRole('heading', { name: /tổng quan/i })).not.toBeInTheDocument();
});
