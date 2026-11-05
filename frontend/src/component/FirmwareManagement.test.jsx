import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import FirmwareManagement from './FirmwareManagement';

const originalFetch = global.fetch;

afterEach(() => {
  global.fetch = originalFetch;
});

function releasesResponse(releases = []) {
  return {
    ok: true,
    status: 200,
    json: async () => ({ success: true, data: releases }),
  };
}

test('uploads a compiled ESP32 binary with the admin token and refreshes release history', async () => {
  const release = {
    version: '1.2.0',
    notes: 'Fix sensor startup',
    sha256: 'a'.repeat(64),
    size_bytes: 4096,
    created_at: '2026-11-05T10:00:00',
  };
  global.fetch = jest.fn()
    .mockResolvedValueOnce(releasesResponse())
    .mockResolvedValueOnce({
      ok: true,
      status: 201,
      json: async () => ({ success: true, data: release }),
    })
    .mockResolvedValueOnce(releasesResponse([release]));

  render(<FirmwareManagement accessToken="admin-token" />);
  fireEvent.change(await screen.findByLabelText('Phiên bản'), { target: { value: '1.2.0' } });
  fireEvent.change(screen.getByLabelText('Ghi chú phát hành'), { target: { value: 'Fix sensor startup' } });
  fireEvent.change(screen.getByLabelText(/File firmware/), {
    target: { files: [new File([new Uint8Array([0xe9, 1, 2])], 'garden.bin')] },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Tải lên và phát hành' }));

  expect(await screen.findByRole('status')).toHaveTextContent('Đã phát hành firmware phiên bản 1.2.0.');
  expect(await screen.findByText('v1.2.0')).toBeInTheDocument();
  await waitFor(() => expect(global.fetch).toHaveBeenCalledTimes(3));
  const [, uploadOptions] = global.fetch.mock.calls[1];
  expect(global.fetch.mock.calls[1][0]).toBe('/api/admin/firmware');
  expect(uploadOptions.method).toBe('POST');
  expect(uploadOptions.headers).toEqual({ Authorization: 'Bearer admin-token' });
  expect(uploadOptions.body.get('version')).toBe('1.2.0');
  expect(uploadOptions.body.get('notes')).toBe('Fix sensor startup');
  expect(uploadOptions.body.get('file').name).toBe('garden.bin');
  expect(global.fetch.mock.calls[2][0]).toBe('/api/admin/firmware');
});

test('rejects non-bin files before calling the upload API', async () => {
  global.fetch = jest.fn().mockResolvedValueOnce(releasesResponse());

  render(<FirmwareManagement accessToken="admin-token" />);
  fireEvent.change(await screen.findByLabelText('Phiên bản'), { target: { value: '1.2.1' } });
  fireEvent.change(screen.getByLabelText(/File firmware/), {
    target: { files: [new File(['source'], 'firmware.ino')] },
  });
  fireEvent.click(screen.getByRole('button', { name: 'Tải lên và phát hành' }));

  expect(await screen.findByRole('alert')).toHaveTextContent('Chọn file firmware ESP32 có đuôi .bin.');
  expect(global.fetch).toHaveBeenCalledTimes(1);
});

test('shows backend errors when release history cannot be loaded', async () => {
  global.fetch = jest.fn().mockResolvedValue({
    ok: false,
    status: 503,
    json: async () => ({ success: false, message: 'Dịch vụ firmware không khả dụng' }),
  });

  render(<FirmwareManagement accessToken="admin-token" />);

  expect(await screen.findByRole('alert')).toHaveTextContent('Dịch vụ firmware không khả dụng');
});
