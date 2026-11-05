import { useCallback, useEffect, useState } from 'react';
import { authenticatedFetch } from '../authenticatedFetch';
import './FirmwareManagement.css';

const API_BASE_URL = process.env.REACT_APP_API_BASE_URL || '';
const MAX_FILE_SIZE = 4 * 1024 * 1024;

function FirmwareManagement({ accessToken }) {
  const [releases, setReleases] = useState([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isUploading, setIsUploading] = useState(false);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');
  const [form, setForm] = useState({ version: '', notes: '', file: null });

  const loadReleases = useCallback(async (signal) => {
    setIsLoading(true);
    setError('');
    try {
      const response = await authenticatedFetch(`${API_BASE_URL}/api/admin/firmware`, {
        headers: { Authorization: `Bearer ${accessToken}` },
        signal,
      }, accessToken);
      const body = await response.json().catch(() => null);
      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể tải lịch sử firmware (HTTP ${response.status}).`);
      }
      if (!Array.isArray(body?.data)) {
        throw new Error('Máy chủ trả về lịch sử firmware không hợp lệ.');
      }
      setReleases(body.data);
    } catch (loadError) {
      if (loadError.name === 'AbortError') return;
      setError(loadError instanceof TypeError
        ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
        : loadError.message || 'Không thể tải lịch sử firmware.');
    } finally {
      if (!signal?.aborted) setIsLoading(false);
    }
  }, [accessToken]);

  useEffect(() => {
    const controller = new AbortController();
    loadReleases(controller.signal);
    return () => controller.abort();
  }, [loadReleases]);

  async function handleUpload(event) {
    event.preventDefault();
    setError('');
    setSuccess('');

    const version = form.version.trim();
    if (!/^[A-Za-z0-9][A-Za-z0-9._+-]{0,63}$/.test(version)) {
      setError('Phiên bản chỉ được gồm chữ, số, dấu chấm, gạch ngang, gạch dưới hoặc dấu cộng.');
      return;
    }
    if (!form.file || !form.file.name.toLowerCase().endsWith('.bin')) {
      setError('Chọn file firmware ESP32 có đuôi .bin.');
      return;
    }
    if (form.file.size > MAX_FILE_SIZE) {
      setError('File firmware không được vượt quá 4 MB.');
      return;
    }

    const upload = new FormData();
    upload.append('version', version);
    upload.append('notes', form.notes.trim());
    upload.append('file', form.file);
    setIsUploading(true);
    try {
      const response = await authenticatedFetch(`${API_BASE_URL}/api/admin/firmware`, {
        method: 'POST',
        headers: { Authorization: `Bearer ${accessToken}` },
        body: upload,
      }, accessToken);
      const body = await response.json().catch(() => null);
      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể tải firmware lên (HTTP ${response.status}).`);
      }
      if (!body?.data?.version) {
        throw new Error('Máy chủ không trả về thông tin phiên bản firmware.');
      }
      setForm({ version: '', notes: '', file: null });
      event.target.reset();
      setSuccess(`Đã phát hành firmware phiên bản ${body.data.version}.`);
      await loadReleases();
    } catch (uploadError) {
      setError(uploadError instanceof TypeError
        ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
        : uploadError.message || 'Không thể tải firmware lên.');
    } finally {
      setIsUploading(false);
    }
  }

  return (
    <section className="firmware-management" aria-labelledby="firmware-title">
      <header className="firmware-management__heading">
        <div>
          <span className="firmware-management__eyebrow">QUẢN LÝ THIẾT BỊ</span>
          <h1 id="firmware-title">Cập nhật firmware OTA</h1>
          <p>Tải bản firmware ESP32 đã biên dịch lên và theo dõi các phiên bản đã phát hành.</p>
        </div>
        <span className="firmware-management__badge">{releases.length} phiên bản</span>
      </header>

      <div className="firmware-management__layout">
        <form className="firmware-management__card" onSubmit={handleUpload}>
          <h2>Phát hành firmware mới</h2>
          <p className="firmware-management__hint">
            Chỉ nhận file .bin ESP32 (tối đa 4 MB). Phiên bản mới tải lên sẽ được thiết bị nhận ở lần kiểm tra tiếp theo.
          </p>
          <label className="firmware-management__field">
            Phiên bản
            <input
              required
              maxLength={64}
              value={form.version}
              onChange={(event) => setForm({ ...form, version: event.target.value })}
              placeholder="Ví dụ: 1.2.0"
            />
          </label>
          <label className="firmware-management__field">
            Ghi chú phát hành
            <textarea
              maxLength={500}
              rows={3}
              value={form.notes}
              onChange={(event) => setForm({ ...form, notes: event.target.value })}
              placeholder="Mô tả thay đổi (không bắt buộc)"
            />
          </label>
          <label className="firmware-management__field">
            File firmware (.bin)
            <input
              required
              type="file"
              accept=".bin,application/octet-stream"
              onChange={(event) => setForm({ ...form, file: event.target.files?.[0] || null })}
            />
            <small>File phải là firmware đã biên dịch cho đúng loại board ESP32.</small>
          </label>
          <button className="firmware-management__submit" type="submit" disabled={isUploading}>
            {isUploading ? 'Đang tải lên...' : 'Tải lên và phát hành'}
          </button>
          {error && <p className="firmware-management__message firmware-management__message--error" role="alert">{error}</p>}
          {success && <p className="firmware-management__message firmware-management__message--success" role="status">{success}</p>}
        </form>

        <section className="firmware-management__card" aria-labelledby="firmware-history-title">
          <div className="firmware-management__history-heading">
            <div>
              <h2 id="firmware-history-title">Lịch sử phát hành</h2>
              <p>Thiết bị tải phiên bản mới nhất chưa cài đặt.</p>
            </div>
            <button
              className="firmware-management__refresh"
              type="button"
              onClick={() => loadReleases()}
              disabled={isLoading}
            >
              {isLoading ? 'Đang tải...' : 'Làm mới'}
            </button>
          </div>
          {isLoading ? <p className="firmware-management__empty">Đang tải lịch sử firmware...</p>
            : releases.length === 0
              ? <p className="firmware-management__empty">Chưa có firmware nào được phát hành.</p>
              : (
                <ol className="firmware-management__release-list">
                  {releases.map((release, index) => (
                    <li className="firmware-management__release" key={release.version}>
                      <div className="firmware-management__release-title">
                        <strong>v{release.version}</strong>
                        {index === 0 && <span>MỚI NHẤT</span>}
                      </div>
                      {release.notes && <p>{release.notes}</p>}
                      <small>
                        {formatDate(release.created_at)} · {formatBytes(release.size_bytes)}
                      </small>
                      <code>SHA-256: {release.sha256}</code>
                    </li>
                  ))}
                </ol>
              )}
        </section>
      </div>

      <aside className="firmware-management__notice">
        <strong>Lưu ý tích hợp ESP32</strong>
        <span>Thiết bị cần gọi GET /api/firmware/check?currentVersion=... định kỳ, sau đó tải file từ download_url. Backend chỉ phân phối firmware, không tự biên dịch mã nguồn hoặc cài đặt OTA trên thiết bị.</span>
      </aside>
    </section>
  );
}

function formatDate(value) {
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? 'Chưa rõ thời gian' : date.toLocaleString('vi-VN');
}

function formatBytes(bytes) {
  if (!Number.isFinite(bytes)) return 'Dung lượng không xác định';
  return `${(bytes / 1024).toFixed(1)} KB`;
}

export default FirmwareManagement;
