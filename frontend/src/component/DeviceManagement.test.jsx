import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import DeviceManagement from './DeviceManagement';

const originalFetch = global.fetch;

afterEach(() => {
  global.fetch = originalFetch;
});

test('registers a device for a user through the admin API', async () => {
  global.fetch = jest.fn()
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({
        success: true,
        data: [{
          id: 'GARDEN-001',
          device_name: 'Vườn ban công',
          status: 'inactive',
          hardware_version: '1.0',
          firmware_version: '2.0',
          last_active_at: null,
        }],
      }),
    })
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({
        success: true,
        message: 'Đăng ký thiết bị thành công',
      }),
    })
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({
        success: true,
        data: [{
          id: 'GARDEN-001',
          device_name: 'Vườn ban công',
          status: 'active',
          hardware_version: '1.0',
          firmware_version: '2.0',
          last_active_at: null,
        }],
      }),
    });

  render(<DeviceManagement accessToken="admin-token" />);
  expect(await screen.findByText('Vườn ban công')).toBeInTheDocument();
  expect(global.fetch).toHaveBeenNthCalledWith(
    1,
    '/api/admin/devices',
    expect.objectContaining({ headers: { Authorization: 'Bearer admin-token' } }),
  );
  fireEvent.click(screen.getByRole('button', { name: 'Chọn' }));
  expect(screen.getByLabelText('Mã thiết bị')).toHaveValue('GARDEN-001');
  fireEvent.change(screen.getByLabelText(/ID người dùng/), {
    target: { value: '12' },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Đăng ký thiết bị' }));

  expect(await screen.findByRole('status')).toHaveTextContent('Đăng ký thiết bị thành công');
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(3));
  expect(global.fetch).toHaveBeenNthCalledWith(
    2,
    '/api/devices/GARDEN-001/register',
    expect.objectContaining({
      method: 'POST',
      headers: {
        Authorization: 'Bearer admin-token',
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ userId: 12 }),
    }),
  );
  expect(await screen.findByText('Đã đăng ký')).toBeInTheDocument();
});

test('shows the backend error when the device or user cannot be registered', async () => {
  global.fetch = jest.fn()
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({ success: true, data: [] }),
    })
    .mockResolvedValueOnce({
      ok: false,
      status: 404,
      json: async () => ({ success: false, message: 'Thiết bị không tồn tại' }),
    });

  render(<DeviceManagement accessToken="admin-token" />);
  fireEvent.change(await screen.findByLabelText('Mã thiết bị'), {
    target: { value: 'UNKNOWN' },
  });
  fireEvent.change(screen.getByLabelText(/ID người dùng/), {
    target: { value: '12' },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Đăng ký thiết bị' }));

  expect(await screen.findByRole('alert')).toHaveTextContent('Thiết bị không tồn tại');
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(2));
});

test('shows an error when the device listing API fails', async () => {
  global.fetch = jest.fn().mockResolvedValue({
    ok: false,
    status: 503,
    json: async () => ({ success: false, message: 'Dịch vụ tạm thời không khả dụng' }),
  });

  render(<DeviceManagement accessToken="admin-token" />);
  expect(await screen.findByRole('alert')).toHaveTextContent('Dịch vụ tạm thời không khả dụng');
  expect(global.fetch).toHaveBeenCalledWith(
    '/api/admin/devices',
    expect.objectContaining({ headers: { Authorization: 'Bearer admin-token' } }),
  );
});

test('creates a device and displays its one-time provisioning credentials', async () => {
  global.fetch = jest.fn()
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({ success: true, data: [] }),
    })
    .mockResolvedValueOnce({
      ok: true,
      status: 201,
      json: async () => ({
        success: true,
        statusCode: 201,
        data: {
          id: 'AA:BB:CC:DD:EE:FF',
          device_name: 'Máy tưới ban công',
          status: 'inactive',
          ble_secret_key: 'ble-secret',
          mqtt_username: 'dev_AABBCCDDEEFF',
          mqtt_password: 'mqtt-secret',
        },
      }),
    })
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({
        success: true,
        data: [{
          id: 'AA:BB:CC:DD:EE:FF',
          device_name: 'Máy tưới ban công',
          status: 'inactive',
          hardware_version: 'ESP32',
          firmware_version: '1.0.0',
          last_active_at: null,
        }],
      }),
    });

  render(<DeviceManagement accessToken="admin-token" />);
  fireEvent.click(await screen.findByRole('button', { name: 'Thêm thiết bị' }));
  fireEvent.change(screen.getByLabelText('Địa chỉ MAC'), {
    target: { value: 'AA:BB:CC:DD:EE:FF' },
  });
  fireEvent.change(screen.getByLabelText('Tên thiết bị'), {
    target: { value: 'Máy tưới ban công' },
  });
  fireEvent.change(screen.getByLabelText('Phiên bản phần cứng'), {
    target: { value: 'ESP32' },
  });
  fireEvent.change(screen.getByLabelText('Phiên bản firmware'), {
    target: { value: '1.0.0' },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Tạo thiết bị' }));

  expect(await screen.findByText('dev_AABBCCDDEEFF')).toBeInTheDocument();
  expect(screen.getByText('mqtt-secret')).toBeInTheDocument();
  expect(screen.getByText('ble-secret')).toBeInTheDocument();
  expect(global.fetch).toHaveBeenNthCalledWith(
    2,
    '/api/admin/devices',
    expect.objectContaining({
      method: 'POST',
      headers: {
        Authorization: 'Bearer admin-token',
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        id: 'AA:BB:CC:DD:EE:FF',
        deviceName: 'Máy tưới ban công',
        hardwareVersion: 'ESP32',
        firmwareVersion: '1.0.0',
      }),
    }),
  );
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(3));
});

test('shows backend errors when device creation fails', async () => {
  global.fetch = jest.fn()
    .mockResolvedValueOnce({
      ok: true,
      status: 200,
      json: async () => ({ success: true, data: [] }),
    })
    .mockResolvedValueOnce({
      ok: false,
      status: 409,
      json: async () => ({ success: false, message: 'Thiết bị đã tồn tại' }),
    });

  render(<DeviceManagement accessToken="admin-token" />);
  fireEvent.click(await screen.findByRole('button', { name: 'Thêm thiết bị' }));
  fireEvent.change(screen.getByLabelText('Địa chỉ MAC'), {
    target: { value: 'AA:BB:CC:DD:EE:FF' },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Tạo thiết bị' }));

  expect(await screen.findByRole('alert')).toHaveTextContent('Thiết bị đã tồn tại');
  expect(global.fetch).toHaveBeenCalledTimes(2);
});
