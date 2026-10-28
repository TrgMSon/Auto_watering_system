import { act, fireEvent, render, screen, waitFor } from '@testing-library/react';
import App from './App';

const originalFetch = global.fetch;

beforeEach(() => {
  localStorage.clear();
  window.history.replaceState({}, '', '/');
});

afterEach(() => {
  global.fetch = originalFetch;
  jest.useRealTimers();
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

test('opens the user-management page from the administrator navigation', async () => {
  localStorage.setItem('garden-admin-auth', JSON.stringify({
    accessToken: 'admin-token',
    refreshToken: 'refresh-token',
    role: 'ADMIN',
    user: { email: 'admin@example.com', fullName: 'Garden Admin' },
  }));
  global.fetch = jest.fn().mockResolvedValue({
    ok: true,
    status: 200,
    json: async () => ({
      success: true,
      data: [{
        id: 8,
        email: 'user@example.com',
        phone_number: '0900000000',
        role: 'USER',
        status: 'ACTIVE',
        created_at: '2026-10-28T08:30:00',
      }],
    }),
  });

  render(<App />);
  fireEvent.click(screen.getByRole('button', { name: 'Người dùng' }));

  expect(await screen.findByRole('heading', { name: 'Quản lý người dùng' })).toBeInTheDocument();
  expect(await screen.findByText('user@example.com')).toBeInTheDocument();
  expect(global.fetch).toHaveBeenCalledWith(
    '/api/admin/users',
    expect.objectContaining({ headers: { Authorization: 'Bearer admin-token' } }),
  );
});

test('automatically logs out when the access token expires', async () => {
  jest.useFakeTimers();
  jest.setSystemTime(new Date('2026-10-28T09:00:00Z'));
  const exp = Math.floor((Date.now() + 10_000) / 1000);
  localStorage.setItem('garden-admin-auth', JSON.stringify({
    accessToken: `header.${btoa(JSON.stringify({ exp }))}.signature`,
    refreshToken: 'refresh-token',
    role: 'ADMIN',
    user: { email: 'admin@example.com' },
  }));

  render(<App />);
  expect(screen.getByRole('heading', { name: /tổng quan/i })).toBeInTheDocument();

  await act(async () => {
    jest.advanceTimersByTime(10_001);
  });

  expect(screen.getByRole('heading', { name: /chào mừng trở lại/i })).toBeInTheDocument();
  expect(localStorage.getItem('garden-admin-auth')).toBeNull();
});

test('automatically logs out when an authenticated API returns 401', async () => {
  localStorage.setItem('garden-admin-auth', JSON.stringify({
    accessToken: 'admin-token',
    refreshToken: 'refresh-token',
    role: 'ADMIN',
    user: { email: 'admin@example.com' },
  }));
  localStorage.setItem('garden-admin-device-id', 'GARDEN-001');
  global.fetch = jest.fn().mockResolvedValue({
    ok: false,
    status: 401,
    json: async () => ({ success: false, message: 'Token hết hạn' }),
  });

  render(<App />);

  expect(await screen.findByRole('heading', { name: /chào mừng trở lại/i })).toBeInTheDocument();
  expect(localStorage.getItem('garden-admin-auth')).toBeNull();
});
