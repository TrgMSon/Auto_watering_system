import { useCallback, useEffect, useMemo, useState } from 'react';
import './UserManagement.css';
import { authenticatedFetch } from '../authenticatedFetch';

const API_BASE_URL = process.env.REACT_APP_API_BASE_URL || '';

function UserManagement({ accessToken }) {
  const [users, setUsers] = useState([]);
  const [search, setSearch] = useState('');
  const [isLoading, setIsLoading] = useState(true);
  const [busyUserId, setBusyUserId] = useState(null);
  const [error, setError] = useState('');
  const [message, setMessage] = useState('');

  const loadUsers = useCallback(async (signal) => {
    setIsLoading(true);
    setError('');

    try {
      const response = await authenticatedFetch(`${API_BASE_URL}/api/admin/users`, {
        headers: { Authorization: `Bearer ${accessToken}` },
        signal,
      }, accessToken);
      const body = await response.json().catch(() => null);

      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể tải danh sách người dùng (HTTP ${response.status}).`);
      }
      if (!Array.isArray(body?.data)) {
        throw new Error('Máy chủ trả về danh sách người dùng không hợp lệ.');
      }

      setUsers(body.data);
    } catch (loadError) {
      if (loadError.name === 'AbortError') return;
      setError(
        loadError instanceof TypeError
          ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
          : loadError.message || 'Không thể tải danh sách người dùng.',
      );
    } finally {
      if (!signal?.aborted) setIsLoading(false);
    }
  }, [accessToken]);

  useEffect(() => {
    const controller = new AbortController();
    loadUsers(controller.signal);
    return () => controller.abort();
  }, [loadUsers]);

  const visibleUsers = useMemo(() => {
    const normalizedSearch = search.trim().toLocaleLowerCase('vi');
    if (!normalizedSearch) return users;
    return users.filter((user) => (
      [user.email, user.phone_number, String(user.id)]
        .some((value) => String(value || '').toLocaleLowerCase('vi').includes(normalizedSearch))
    ));
  }, [search, users]);

  const activeCount = users.filter((user) => user.status?.toUpperCase() === 'ACTIVE').length;
  const lockedCount = users.filter((user) => user.status?.toUpperCase() === 'LOCKED').length;

  async function handleStatusChange(user) {
    const action = user.status?.toUpperCase() === 'LOCKED' ? 'unlock' : 'lock';
    setBusyUserId(user.id);
    setError('');
    setMessage('');

    try {
      const response = await authenticatedFetch(
        `${API_BASE_URL}/api/admin/users/${encodeURIComponent(user.id)}/${action}`,
        {
          method: 'POST',
          headers: { Authorization: `Bearer ${accessToken}` },
        },
        accessToken,
      );
      const body = await response.json().catch(() => null);

      if (response.status === 401 || response.status === 403) {
        throw new Error('Phiên đăng nhập hết hạn hoặc tài khoản không có quyền quản trị.');
      }
      if (!response.ok || body?.success === false) {
        throw new Error(body?.message || `Không thể cập nhật tài khoản (HTTP ${response.status}).`);
      }

      setMessage(body?.message || (action === 'lock'
        ? 'Đã khóa tài khoản người dùng.'
        : 'Đã mở khóa tài khoản người dùng.'));
      await loadUsers();
    } catch (actionError) {
      setError(
        actionError instanceof TypeError
          ? 'Không thể kết nối máy chủ. Kiểm tra backend và cấu hình API.'
          : actionError.message || 'Không thể cập nhật trạng thái tài khoản.',
      );
    } finally {
      setBusyUserId(null);
    }
  }

  return (
    <section className="user-management">
      <header className="user-management__heading">
        <div>
          <span className="eyebrow">QUẢN TRỊ HỆ THỐNG</span>
          <h1>Quản lý người dùng</h1>
          <p>Tra cứu tài khoản và kiểm soát quyền đăng nhập của người dùng.</p>
        </div>
        <button
          className="user-management__refresh"
          type="button"
          onClick={() => loadUsers()}
          disabled={isLoading}
        >
          {isLoading ? 'Đang tải…' : 'Làm mới'}
        </button>
      </header>

      <div className="user-management__summary" aria-label="Thống kê người dùng">
        <SummaryCard label="Tổng tài khoản" value={users.length} />
        <SummaryCard label="Đang hoạt động" value={activeCount} tone="active" />
        <SummaryCard label="Đang khóa" value={lockedCount} tone="locked" />
      </div>

      <section className="user-management__list" aria-labelledby="user-list-title">
        <div className="user-management__list-heading">
          <div>
            <h2 id="user-list-title">Danh sách tài khoản</h2>
            <p>Chỉ tài khoản USER có thể được khóa hoặc mở khóa.</p>
          </div>
          <label className="user-management__search">
            <span className="sr-only">Tìm người dùng</span>
            <input
              type="search"
              value={search}
              onChange={(event) => setSearch(event.target.value)}
              placeholder="Tìm email, số điện thoại hoặc ID"
            />
          </label>
        </div>

        {error && <div className="user-management__message user-management__message--error" role="alert">{error}</div>}
        {message && <div className="user-management__message user-management__message--success" role="status">{message}</div>}

        {isLoading && users.length === 0 ? (
          <p className="user-management__empty" role="status">Đang tải danh sách người dùng…</p>
        ) : error && users.length === 0 ? (
          <p className="user-management__empty">Thử làm mới sau khi kiểm tra kết nối backend.</p>
        ) : visibleUsers.length === 0 ? (
          <p className="user-management__empty">
            {users.length ? 'Không tìm thấy tài khoản phù hợp.' : 'Chưa có tài khoản nào.'}
          </p>
        ) : (
          <div className="user-management__table-wrap">
            <table className="user-management__table">
              <thead>
                <tr>
                  <th>ID</th>
                  <th>Email</th>
                  <th>Số điện thoại</th>
                  <th>Vai trò</th>
                  <th>Trạng thái</th>
                  <th>Ngày tạo</th>
                  <th><span className="sr-only">Thao tác</span></th>
                </tr>
              </thead>
              <tbody>
                {visibleUsers.map((user) => {
                  const role = user.role?.toUpperCase();
                  const status = user.status?.toUpperCase();
                  const canManage = role === 'USER' && ['ACTIVE', 'LOCKED'].includes(status);

                  return (
                    <tr key={user.id}>
                      <td className="user-management__id">{user.id}</td>
                      <td>{user.email || '—'}</td>
                      <td>{user.phone_number || '—'}</td>
                      <td><span className={`user-management__role user-management__role--${role?.toLowerCase() || 'unknown'}`}>{role || 'Không xác định'}</span></td>
                      <td><AccountStatus status={status} /></td>
                      <td>{formatCreatedAt(user.created_at)}</td>
                      <td>
                        {canManage ? (
                          <button
                            className={`user-management__action ${status === 'LOCKED' ? '' : 'user-management__action--danger'}`}
                            type="button"
                            onClick={() => handleStatusChange(user)}
                            disabled={busyUserId !== null}
                          >
                            {busyUserId === user.id
                              ? 'Đang xử lý…'
                              : status === 'LOCKED' ? 'Mở khóa' : 'Khóa'}
                          </button>
                        ) : (
                          <span className="user-management__protected">Không thể thay đổi</span>
                        )}
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        )}
      </section>
    </section>
  );
}

function SummaryCard({ label, value, tone = '' }) {
  return (
    <article className={`user-management__summary-card ${tone ? `user-management__summary-card--${tone}` : ''}`}>
      <span>{label}</span>
      <strong>{value}</strong>
    </article>
  );
}

function AccountStatus({ status }) {
  const labels = {
    ACTIVE: 'Đang hoạt động',
    LOCKED: 'Đang khóa',
    PENDING: 'Chờ kích hoạt',
  };
  return (
    <span className={`user-management__status user-management__status--${status?.toLowerCase() || 'unknown'}`}>
      <i />
      {labels[status] || status || 'Không xác định'}
    </span>
  );
}

function formatCreatedAt(value) {
  if (!value) return '—';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return 'Không xác định';
  return new Intl.DateTimeFormat('vi-VN', {
    dateStyle: 'medium',
    timeStyle: 'short',
  }).format(date);
}

export default UserManagement;
