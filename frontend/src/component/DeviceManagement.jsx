import { useCallback, useEffect, useState } from 'react';
import './DeviceManagement.css';

const API_BASE_URL = process.env.REACT_APP_API_BASE_URL || '';

function DeviceManagement({ accessToken }) {
  const [devices, setDevices] = useState([]);
  const [isLoadingDevices, setIsLoadingDevices] = useState(true);
  const [devicesError, setDevicesError] = useState('');
  const [showCreateForm, setShowCreateForm] = useState(false);
  const [createDevice, setCreateDevice] = useState({
    id: '',
    deviceName: '',
    hardwareVersion: '',
    firmwareVersion: '',
  });
  const [isCreating, setIsCreating] = useState(false);
  const [createError, setCreateError] = useState('');
  const [createdCredentials, setCreatedCredentials] = useState(null);
  const [deviceId, setDeviceId] = useState('');
  const [userId, setUserId] = useState('');
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  const loadDevices = useCallback(async (signal) => {
    setIsLoadingDevices(true);
    setDevicesError('');

    try {
      const response = await fetch(`${API_BASE_URL}/api/admin/devices`, {
        headers: { Authorization: `Bearer ${accessToken}` },
        signal,
      });
      const body = await response.json().catch(() => null);

      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể tải danh sách thiết bị (HTTP ${response.status}).`);
      }
      if (!Array.isArray(body?.data)) {
        throw new Error('Máy chủ trả về danh sách thiết bị không hợp lệ.');
      }

      setDevices(body.data);
    } catch (loadError) {
      if (loadError.name === 'AbortError') return;
      setDevicesError(
        loadError instanceof TypeError
          ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
          : loadError.message || 'Không thể tải danh sách thiết bị.',
      );
    } finally {
      if (!signal?.aborted) setIsLoadingDevices(false);
    }
  }, [accessToken]);

  useEffect(() => {
    const controller = new AbortController();
    loadDevices(controller.signal);
    return () => controller.abort();
  }, [loadDevices]);

  async function handleCreateDevice(event) {
    event.preventDefault();
    setCreateError('');
    setCreatedCredentials(null);

    const payload = {
      id: createDevice.id.trim(),
      deviceName: createDevice.deviceName.trim() || null,
      hardwareVersion: createDevice.hardwareVersion.trim() || null,
      firmwareVersion: createDevice.firmwareVersion.trim() || null,
    };
    setIsCreating(true);
    try {
      const response = await fetch(`${API_BASE_URL}/api/admin/devices`, {
        method: 'POST',
        headers: {
          Authorization: `Bearer ${accessToken}`,
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(payload),
      });
      const body = await response.json().catch(() => null);

      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể thêm thiết bị (HTTP ${response.status}).`);
      }
      if (!body?.data?.id || !body.data.mqtt_username || !body.data.mqtt_password) {
        throw new Error('Thiết bị đã được tạo nhưng máy chủ không trả đủ thông tin MQTT. Hãy kiểm tra backend trước khi nạp firmware.');
      }

      setCreatedCredentials(body.data);
      setCreateDevice({
        id: '',
        deviceName: '',
        hardwareVersion: '',
        firmwareVersion: '',
      });
      setShowCreateForm(false);
      await loadDevices();
    } catch (createDeviceError) {
      setCreateError(
        createDeviceError instanceof TypeError
          ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
          : createDeviceError.message || 'Không thể thêm thiết bị.',
      );
    } finally {
      setIsCreating(false);
    }
  }

  async function handleSubmit(event) {
    event.preventDefault();
    setError('');
    setSuccess('');

    const normalizedDeviceId = deviceId.trim();
    const normalizedUserId = Number(userId);
    if (!normalizedDeviceId || !Number.isSafeInteger(normalizedUserId) || normalizedUserId < 1) {
      setError('Nhập mã thiết bị và ID người dùng hợp lệ.');
      return;
    }

    setIsSubmitting(true);
    try {
      const response = await fetch(
        `${API_BASE_URL}/api/devices/${encodeURIComponent(normalizedDeviceId)}/register`,
        {
          method: 'POST',
          headers: {
            Authorization: `Bearer ${accessToken}`,
            'Content-Type': 'application/json',
          },
          body: JSON.stringify({ userId: normalizedUserId }),
        },
      );
      const body = await response.json().catch(() => null);

      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể đăng ký thiết bị (HTTP ${response.status}).`);
      }
      if (!body || typeof body !== 'object') {
        throw new Error('Máy chủ trả về phản hồi không hợp lệ.');
      }

      setSuccess(body.message || 'Đăng ký thiết bị thành công.');
      await loadDevices();
    } catch (submitError) {
      setError(
        submitError instanceof TypeError
          ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
          : submitError.message || 'Không thể đăng ký thiết bị.',
      );
    } finally {
      setIsSubmitting(false);
    }
  }

  return (
    <section className="device-management">
      <header className="device-management__heading">
        <div>
          <span className="eyebrow">QUẢN TRỊ HỆ THỐNG</span>
          <h1>Quản lý thiết bị</h1>
          <p>Theo dõi thiết bị đã có trong hệ thống và gán thiết bị cho tài khoản người dùng.</p>
        </div>
        <span className="device-management__badge">
          <DeviceManagementIcon /> {devices.length} thiết bị
        </span>
      </header>

      <div className="device-management__create-toolbar">
        <div>
          <strong>Khởi tạo thiết bị</strong>
          <p>Thêm thiết bị mới bằng địa chỉ MAC và tạo thông tin provisioning.</p>
        </div>
        <button
          className="device-management__create-toggle"
          type="button"
          onClick={() => {
            setShowCreateForm((visible) => !visible);
            setCreateError('');
          }}
          aria-expanded={showCreateForm}
          aria-controls="create-device-form"
        >
          {showCreateForm ? 'Đóng biểu mẫu' : 'Thêm thiết bị'}
        </button>
      </div>

      {showCreateForm && (
        <form
          id="create-device-form"
          className="device-management__create-form"
          onSubmit={handleCreateDevice}
        >
          <div className="device-management__create-form-heading">
            <h2>Thông tin thiết bị mới</h2>
            <p>Địa chỉ MAC là bắt buộc; các trường khác có thể để trống.</p>
          </div>
          <div className="device-management__create-fields">
            <label className="device-management__field" htmlFor="new-device-mac">
              Địa chỉ MAC
              <input
                id="new-device-mac"
                autoComplete="off"
                required
                pattern="(?:[0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"
                title="Nhập MAC gồm 6 cặp ký tự hex, ví dụ AA:BB:CC:DD:EE:FF"
                value={createDevice.id}
                onChange={(event) => setCreateDevice({ ...createDevice, id: event.target.value })}
                placeholder="AA:BB:CC:DD:EE:FF"
              />
            </label>
            <label className="device-management__field" htmlFor="new-device-name">
              Tên thiết bị
              <input
                id="new-device-name"
                value={createDevice.deviceName}
                onChange={(event) => setCreateDevice({ ...createDevice, deviceName: event.target.value })}
                placeholder="Ví dụ: Máy tưới ban công"
              />
            </label>
            <label className="device-management__field" htmlFor="new-device-hardware">
              Phiên bản phần cứng
              <input
                id="new-device-hardware"
                value={createDevice.hardwareVersion}
                onChange={(event) => setCreateDevice({ ...createDevice, hardwareVersion: event.target.value })}
                placeholder="ESP32"
              />
            </label>
            <label className="device-management__field" htmlFor="new-device-firmware">
              Phiên bản firmware
              <input
                id="new-device-firmware"
                value={createDevice.firmwareVersion}
                onChange={(event) => setCreateDevice({ ...createDevice, firmwareVersion: event.target.value })}
                placeholder="1.0.0"
              />
            </label>
          </div>
          {createError && (
            <div className="device-management__message device-management__message--error" role="alert">
              {createError}
            </div>
          )}
          <button className="device-management__submit" type="submit" disabled={isCreating}>
            {isCreating ? 'Đang tạo thiết bị…' : 'Tạo thiết bị'}
          </button>
        </form>
      )}

      {createdCredentials && (
        <section className="device-management__credentials" aria-labelledby="created-device-title">
          <div className="device-management__credentials-heading">
            <div>
              <span className="device-management__credentials-kicker">THIẾT BỊ ĐÃ KHỞI TẠO</span>
              <h2 id="created-device-title">{createdCredentials.id}</h2>
              <p>Lưu lại thông tin này ngay. Bí mật chỉ được trả về trong phản hồi tạo thiết bị.</p>
            </div>
            <button
              className="device-management__credentials-dismiss"
              type="button"
              onClick={() => setCreatedCredentials(null)}
              aria-label="Ẩn thông tin provisioning"
            >
              Ẩn
            </button>
          </div>
          <div className="device-management__credential-grid">
            <Credential label="Khóa BLE" value={createdCredentials.ble_secret_key} />
            <Credential label="MQTT username" value={createdCredentials.mqtt_username} />
            <Credential label="MQTT password" value={createdCredentials.mqtt_password} />
          </div>
        </section>
      )}

      <section className="device-management__list" aria-labelledby="device-list-title">
        <div className="device-management__list-heading">
          <div>
            <h2 id="device-list-title">Danh sách thiết bị</h2>
            <p>Chọn một thiết bị để điền mã vào biểu mẫu đăng ký.</p>
          </div>
          <button
            className="device-management__refresh"
            type="button"
            onClick={() => loadDevices()}
            disabled={isLoadingDevices}
          >
            {isLoadingDevices ? 'Đang tải…' : 'Làm mới'}
          </button>
        </div>

        {devicesError && (
          <div className="device-management__message device-management__message--error" role="alert">
            {devicesError}
          </div>
        )}

        {isLoadingDevices && devices.length === 0 ? (
          <p className="device-management__list-state" role="status">Đang tải danh sách thiết bị…</p>
        ) : devicesError && devices.length === 0 ? (
          <p className="device-management__list-state">Thử làm mới sau khi kiểm tra kết nối backend.</p>
        ) : devices.length === 0 ? (
          <p className="device-management__list-state">Chưa có thiết bị nào được tạo trong hệ thống.</p>
        ) : (
          <div className="device-management__table-wrap">
            <table className="device-management__table">
              <thead>
                <tr>
                  <th>Mã thiết bị</th>
                  <th>Tên thiết bị</th>
                  <th>Trạng thái</th>
                  <th>Phiên bản</th>
                  <th>Hoạt động gần nhất</th>
                  <th><span className="sr-only">Thao tác</span></th>
                </tr>
              </thead>
              <tbody>
                {devices.map((device) => (
                  <tr key={device.id}>
                    <td className="device-management__device-id">{device.id}</td>
                    <td>{device.device_name || '—'}</td>
                    <td><DeviceStatus status={device.status} /></td>
                    <td>
                      <span className="device-management__versions">
                        HW {device.hardware_version || '—'} · FW {device.firmware_version || '—'}
                      </span>
                    </td>
                    <td>{formatLastActive(device.last_active_at)}</td>
                    <td>
                      <button
                        className="device-management__select"
                        type="button"
                        onClick={() => {
                          setDeviceId(device.id);
                          setError('');
                          setSuccess('');
                        }}
                      >
                        Chọn
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </section>

      <div className="device-management__layout">
        <form className="device-management__form" onSubmit={handleSubmit}>
          <div className="device-management__form-heading">
            <span className="device-management__form-icon"><DeviceManagementIcon /></span>
            <div>
              <h2>Gán thiết bị cho người dùng</h2>
              <p>Thông tin được xác thực trực tiếp với máy chủ.</p>
            </div>
          </div>

          <label className="device-management__field" htmlFor="managed-device-id">
            Mã thiết bị
            <input
              id="managed-device-id"
              autoComplete="off"
              required
              value={deviceId}
              onChange={(event) => setDeviceId(event.target.value)}
              placeholder="Ví dụ: GARDEN-001"
            />
          </label>

          <label className="device-management__field" htmlFor="managed-user-id">
            ID người dùng
            <input
              id="managed-user-id"
              type="number"
              min="1"
              step="1"
              required
              value={userId}
              onChange={(event) => setUserId(event.target.value)}
              placeholder="Nhập ID tài khoản USER"
            />
            <small>Người dùng phải tồn tại và có vai trò USER.</small>
          </label>

          {error && <div className="device-management__message device-management__message--error" role="alert">{error}</div>}
          {success && <div className="device-management__message device-management__message--success" role="status">{success}</div>}

          <button className="device-management__submit" type="submit" disabled={isSubmitting}>
            {isSubmitting ? 'Đang đăng ký…' : 'Đăng ký thiết bị'}
          </button>
        </form>

        <aside className="device-management__info">
          <span className="device-management__info-icon"><InfoIcon /></span>
          <h2>Lưu ý trước khi đăng ký</h2>
          <ul>
            <li>Thiết bị cần tồn tại trong hệ thống trước khi gán.</li>
            <li>Không thể đăng ký lại thiết bị đang hoạt động.</li>
            <li>Sau khi gán, tài khoản người dùng có thể truy cập thiết bị trong ứng dụng.</li>
          </ul>
          <p>Danh sách thiết bị được tải từ backend. Người dùng phải tồn tại và có vai trò USER.</p>
        </aside>
      </div>
    </section>
  );
}

function DeviceStatus({ status }) {
  const normalizedStatus = status?.toLowerCase();
  const statusLabels = {
    active: 'Đã đăng ký',
    inactive: 'Chưa đăng ký',
    online: 'Đang trực tuyến',
    offline: 'Ngoại tuyến',
  };
  const label = statusLabels[normalizedStatus] || status || 'Chưa xác định';
  const isActive = normalizedStatus === 'active' || normalizedStatus === 'online';

  return (
    <span className={`device-management__status ${isActive ? 'device-management__status--active' : ''}`}>
      <i />
      {label}
    </span>
  );
}

function Credential({ label, value }) {
  if (!value) return null;
  return (
    <div className="device-management__credential">
      <span>{label}</span>
      <code>{value}</code>
    </div>
  );
}

function formatLastActive(value) {
  if (!value) return 'Chưa ghi nhận';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return 'Không xác định';
  return new Intl.DateTimeFormat('vi-VN', {
    dateStyle: 'medium',
    timeStyle: 'short',
  }).format(date);
}

function DeviceManagementIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <rect x="4" y="4" width="16" height="16" rx="4" stroke="currentColor" strokeWidth="1.7" />
      <path d="M9 9h6v6H9zM12 2v2m0 16v2M2 12h2m16 0h2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" />
    </svg>
  );
}

function InfoIcon() {
  return (
    <svg viewBox="0 0 24 24" fill="none" aria-hidden="true">
      <circle cx="12" cy="12" r="9" stroke="currentColor" strokeWidth="1.7" />
      <path d="M12 11v5m0-8h.01" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" />
    </svg>
  );
}

export default DeviceManagement;
