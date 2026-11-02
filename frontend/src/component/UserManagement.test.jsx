import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import UserManagement from './UserManagement';

const originalFetch = global.fetch;

afterEach(() => {
  global.fetch = originalFetch;
});

function response(data, message = '') {
  return {
    ok: true,
    status: 200,
    json: async () => ({ success: true, data, message }),
  };
}

const users = [
  {
    id: 8,
    email: 'user@example.com',
    phone_number: '0900000000',
    role: 'USER',
    status: 'ACTIVE',
    created_at: '2026-10-28T08:30:00',
  },
  {
    id: 9,
    email: 'locked@example.com',
    phone_number: null,
    role: 'USER',
    status: 'LOCKED',
    created_at: '2026-10-27T09:00:00',
  },
  {
    id: 1,
    email: 'admin@example.com',
    phone_number: null,
    role: 'ADMIN',
    status: 'ACTIVE',
    created_at: '2026-10-26T09:00:00',
  },
];

test('loads users, shows account status, and protects admin accounts from status changes', async () => {
  global.fetch = jest.fn().mockResolvedValue(response(users));

  render(<UserManagement accessToken="admin-token" />);

  expect(await screen.findByText('user@example.com')).toBeInTheDocument();
  expect(screen.getAllByText('Đang hoạt động')).toHaveLength(3);
  expect(screen.getAllByText('Đang khóa')).toHaveLength(2);
  expect(screen.getByText('ADMIN')).toBeInTheDocument();
  expect(screen.getByText('Không thể thay đổi')).toBeInTheDocument();
  expect(global.fetch).toHaveBeenCalledWith(
    '/api/admin/users',
    expect.objectContaining({ headers: { Authorization: 'Bearer admin-token' } }),
  );
});

test('locks a user and refreshes the account list', async () => {
  global.fetch = jest.fn()
    .mockResolvedValueOnce(response([users[0]]))
    .mockResolvedValueOnce(response(null, 'Đã khóa tài khoản người dùng'))
    .mockResolvedValueOnce(response([{ ...users[0], status: 'LOCKED' }]));

  render(<UserManagement accessToken="admin-token" />);
  fireEvent.click(await screen.findByRole('button', { name: 'Khóa' }));

  expect(await screen.findByText('Đã khóa tài khoản người dùng')).toBeInTheDocument();
  expect(screen.getAllByText('Đang khóa')).toHaveLength(2);
  expect(global.fetch).toHaveBeenNthCalledWith(
    2,
    '/api/admin/users/8/lock',
    expect.objectContaining({
      method: 'POST',
      headers: { Authorization: 'Bearer admin-token' },
    }),
  );
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(3));
});

test('unlocks a locked user and refreshes the account list', async () => {
  const lockedUser = { ...users[1] };
  global.fetch = jest.fn()
    .mockResolvedValueOnce(response([lockedUser]))
    .mockResolvedValueOnce(response(null, 'Đã mở khóa tài khoản người dùng'))
    .mockResolvedValueOnce(response([{ ...lockedUser, status: 'ACTIVE' }]));

  render(<UserManagement accessToken="admin-token" />);
  fireEvent.click(await screen.findByRole('button', { name: 'Mở khóa' }));

  expect(await screen.findByText('Đã mở khóa tài khoản người dùng')).toBeInTheDocument();
  expect(screen.getAllByText('Đang hoạt động')).toHaveLength(2);
  expect(global.fetch).toHaveBeenNthCalledWith(
    2,
    '/api/admin/users/9/unlock',
    expect.objectContaining({ method: 'POST' }),
  );
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(3));
});

test('filters users by email, phone number, or id', async () => {
  global.fetch = jest.fn().mockResolvedValue(response(users));

  render(<UserManagement accessToken="admin-token" />);
  await screen.findByText('user@example.com');
  fireEvent.change(screen.getByRole('searchbox', { name: 'Tìm người dùng' }), {
    target: { value: '0900000000' },
  });

  expect(screen.getByText('user@example.com')).toBeInTheDocument();
  expect(screen.queryByText('locked@example.com')).not.toBeInTheDocument();
  expect(screen.queryByText('admin@example.com')).not.toBeInTheDocument();
});

test('shows an API error rather than an empty user list', async () => {
  global.fetch = jest.fn().mockResolvedValue({
    ok: false,
    status: 503,
    json: async () => ({ success: false, message: 'Dịch vụ người dùng tạm thời không khả dụng' }),
  });

  render(<UserManagement accessToken="admin-token" />);
  expect(await screen.findByRole('alert')).toHaveTextContent(
    'Dịch vụ người dùng tạm thời không khả dụng',
  );
});
