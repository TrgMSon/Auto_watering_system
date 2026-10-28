import { useCallback, useEffect, useMemo, useState } from 'react';
import DeviceManagement from './DeviceManagement';
import UserManagement from './UserManagement';
import { authenticatedFetch } from '../authenticatedFetch';

const API_BASE_URL = process.env.REACT_APP_API_BASE_URL || '';
const DEVICE_STORAGE_KEY = 'garden-admin-device-id';
const REFRESH_INTERVAL = 30_000;

const RANGE_OPTIONS = [
  { value: '24h', label: '24 giờ qua' },
  { value: '7d', label: '7 ngày qua' },
  { value: '30d', label: '30 ngày qua' },
];

function unwrapResponse(body) {
  if (body && typeof body === 'object' && 'success' in body) {
    if (!body.success) {
      throw new Error(body.message || 'Máy chủ không thể trả dữ liệu thống kê.');
    }
    return body.data;
  }
  return body;
}

function normalizeRows(data) {
  if (Array.isArray(data)) return data;
  if (Array.isArray(data?.items)) return data.items;
  if (Array.isArray(data?.content)) return data.content;
  if (Array.isArray(data?.data)) return data.data;
  return [];
}

function valueOf(row, ...keys) {
  for (const key of keys) {
    const value = row?.[key];
    if (value !== undefined && value !== null && value !== '') {
      const number = Number(value);
      if (Number.isFinite(number)) return number;
    }
  }
  return null;
}

function timeOf(row, index) {
  return row?.timestamp || row?.recordedAt || row?.recorded_at
    || row?.timeLabel || row?.time_label || row?.time || `Mốc ${index + 1}`;
}

function normalizeEnvironmental(data) {
  return normalizeRows(data).map((row, index) => ({
    label: timeOf(row, index),
    soil: valueOf(row, 'soilMoisture', 'soil_moisture', 'avgSoil', 'avg_soil'),
    humidity: valueOf(row, 'humidity', 'airHumidity', 'air_humidity', 'avgHumid', 'avg_humid'),
    temperature: valueOf(row, 'temperature', 'airTemperature', 'air_temperature', 'avgTemp', 'avg_temp'),
  })).filter((row) => row.soil !== null || row.humidity !== null || row.temperature !== null);
}

function normalizeWater(data) {
  return normalizeRows(data).map((row, index) => ({
    label: timeOf(row, index),
    water: valueOf(row, 'waterLevel', 'water_level', 'avgWaterLevel', 'avg_water_level'),
  })).filter((row) => row.water !== null);
}

async function fetchStats(path, token, signal) {
  const response = await authenticatedFetch(`${API_BASE_URL}${path}`, {
    headers: { Authorization: `Bearer ${token}` },
    signal,
  }, token);
  const body = await response.json().catch(() => null);
  if (!response.ok) {
    if (response.status === 401 || response.status === 403) {
      throw new Error('Phiên đăng nhập đã hết hạn hoặc không có quyền xem thống kê.');
    }
    if (response.status === 404) {
      throw new Error('Backend chưa có API thống kê cảm biến cho thiết bị này.');
    }
    throw new Error(body?.message || `Không thể tải thống kê (HTTP ${response.status}).`);
  }
  return unwrapResponse(body);
}

function buildQuery(deviceId, range) {
  const query = new URLSearchParams({ deviceId, range });
  return query.toString();
}

function Home({ auth, onLogout }) {
  const [activePage, setActivePage] = useState('overview');
  const [deviceId, setDeviceId] = useState(
    () => localStorage.getItem(DEVICE_STORAGE_KEY) || '',
  );
  const [deviceInput, setDeviceInput] = useState(deviceId);
  const [range, setRange] = useState('24h');
  const [environmental, setEnvironmental] = useState([]);
  const [water, setWater] = useState([]);
  const [error, setError] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [lastUpdated, setLastUpdated] = useState(null);

  const loadDashboard = useCallback(async (signal) => {
    if (!deviceId.trim()) {
      setEnvironmental([]);
      setWater([]);
      setError('');
      setIsLoading(false);
      return;
    }

    setIsLoading(true);
    setError('');
    const query = buildQuery(deviceId.trim(), range);
    try {
      const [environmentalData, waterData] = await Promise.all([
        fetchStats(`/api/admin/statistics/environmental?${query}`, auth.accessToken, signal),
        fetchStats(`/api/admin/statistics/water-level?${query}`, auth.accessToken, signal),
      ]);
      if (signal?.aborted) return;
      setEnvironmental(normalizeEnvironmental(environmentalData));
      setWater(normalizeWater(waterData));
      setLastUpdated(new Date());
    } catch (loadError) {
      if (loadError.name === 'AbortError') return;
      setEnvironmental([]);
      setWater([]);
      setError(loadError.message || 'Không thể tải dữ liệu thống kê.');
    } finally {
      if (!signal?.aborted) setIsLoading(false);
    }
  }, [auth.accessToken, deviceId, range]);

  useEffect(() => {
    const controller = new AbortController();
    loadDashboard(controller.signal);
    return () => controller.abort();
  }, [loadDashboard]);

  useEffect(() => {
    if (!deviceId) return undefined;
    const intervalId = window.setInterval(() => {
      loadDashboard();
    }, REFRESH_INTERVAL);
    return () => window.clearInterval(intervalId);
  }, [deviceId, loadDashboard]);

  const latestEnvironmental = environmental.at(-1);
  const latestWater = water.at(-1);
  const userName = auth.user?.fullName || auth.user?.name || auth.user?.email || 'Quản trị viên';
  const initials = userName.trim().charAt(0).toUpperCase() || 'A';
  const todayLabel = new Intl.DateTimeFormat('vi-VN', {
    weekday: 'long',
    day: 'numeric',
    month: 'long',
    year: 'numeric',
  }).format(new Date()).toLocaleUpperCase('vi-VN');

  const environmentalStats = useMemo(() => [
    { label: 'Độ ẩm đất', value: latestEnvironmental?.soil, unit: '%', color: 'green', icon: <SoilIcon /> },
    { label: 'Độ ẩm không khí', value: latestEnvironmental?.humidity, unit: '%', color: 'blue', icon: <HumidityIcon /> },
    { label: 'Nhiệt độ', value: latestEnvironmental?.temperature, unit: '°C', color: 'orange', icon: <TemperatureIcon /> },
  ], [latestEnvironmental]);

  function handleDeviceSubmit(event) {
    event.preventDefault();
    const nextDeviceId = deviceInput.trim();
    setDeviceInput(nextDeviceId);
    setDeviceId(nextDeviceId);
    if (nextDeviceId) localStorage.setItem(DEVICE_STORAGE_KEY, nextDeviceId);
    else localStorage.removeItem(DEVICE_STORAGE_KEY);
  }

  return (
    <div className="dashboard-shell">
      <aside className="sidebar">
        <a className="brand brand--sidebar" href="/" aria-label="Gardenia - Trang chủ">
          <span className="brand__mark"><LeafIcon /></span>
          <span>gardenia<span className="brand__dot">.</span></span>
        </a>

        <div className="sidebar__section-label">WORKSPACE</div>
        <nav className="side-nav" aria-label="Điều hướng chính">
          <button
            className={`side-nav__item side-nav__button ${activePage === 'overview' ? 'side-nav__item--active' : ''}`}
            onClick={() => setActivePage('overview')}
            aria-current={activePage === 'overview' ? 'page' : undefined}
          >
            <GridIcon /> <span>Tổng quan</span>
          </button>
          <button
            className={`side-nav__item side-nav__button ${activePage === 'devices' ? 'side-nav__item--active' : ''}`}
            onClick={() => setActivePage('devices')}
            aria-current={activePage === 'devices' ? 'page' : undefined}
          >
            <DeviceIcon /> <span>Thiết bị IoT</span>
          </button>
          <button
            className={`side-nav__item side-nav__button ${activePage === 'users' ? 'side-nav__item--active' : ''}`}
            onClick={() => setActivePage('users')}
            aria-current={activePage === 'users' ? 'page' : undefined}
          >
            <UsersIcon /> <span>Người dùng</span>
          </button>
        </nav>

        <div className="sidebar__bottom">
          <div className="sidebar-help">
            <span className="sidebar-help__icon"><HelpIcon /></span>
            <strong>Cần trợ giúp?</strong>
            <p>Đội ngũ Gardenia luôn sẵn sàng hỗ trợ bạn.</p>
            <a href="mailto:support@gardenia.local">Liên hệ hỗ trợ <ArrowIcon /></a>
          </div>
          <button className="sidebar-profile" onClick={onLogout}>
            <span className="avatar">{initials}</span>
            <span className="sidebar-profile__text"><strong>{userName}</strong><small>Quản trị viên</small></span>
            <LogoutIcon />
          </button>
        </div>
      </aside>

      <main className="dashboard-main">
        <header className="topbar">
          <div className="breadcrumbs">
            <span>Workspace</span><b>/</b>
            <strong>{activePage === 'devices'
              ? 'Thiết bị IoT'
              : activePage === 'users' ? 'Người dùng' : 'Tổng quan'}</strong>
          </div>
          <div className="topbar__actions">
            <div className="system-status"><span className="status-dot" /> Hệ thống hoạt động</div>
            <button className="icon-button notification-button" aria-label="Thông báo"><BellIcon /><i /></button>
            <span className="topbar-avatar">{initials}</span>
          </div>
        </header>

        <div className="dashboard-content">
          {activePage === 'devices' ? (
            <DeviceManagement accessToken={auth.accessToken} />
          ) : activePage === 'users' ? (
            <UserManagement accessToken={auth.accessToken} />
          ) : (
            <>
          <section className="page-heading">
            <div>
              <span className="eyebrow">{todayLabel}</span>
              <h1>Tổng quan <span className="heading-sparkle">✳</span></h1>
              <p>Theo dõi tình trạng khu vườn của bạn theo thời gian thực.</p>
            </div>
            <form className="device-picker" onSubmit={handleDeviceSubmit}>
              <label htmlFor="dashboard-device">MÃ THIẾT BỊ</label>
              <div className="device-picker__controls">
                <DeviceIcon />
                <input
                  id="dashboard-device"
                  value={deviceInput}
                  onChange={(event) => setDeviceInput(event.target.value)}
                  placeholder="Nhập mã thiết bị"
                  aria-label="Mã thiết bị"
                />
                <button type="submit" aria-label="Tải thiết bị"><ArrowRightIcon /></button>
              </div>
            </form>
          </section>

          <section className="overview-strip" aria-label="Tình trạng nhanh">
            <div className="overview-strip__item">
              <span className="overview-icon overview-icon--green"><PlantIcon /></span>
              <span><small>THIẾT BỊ ĐANG XEM</small><strong>{deviceId || 'Chưa chọn thiết bị'}</strong></span>
            </div>
            <div className="overview-strip__divider" />
            <div className="overview-strip__item">
              <span className={`status-ring ${deviceId && !error ? 'status-ring--online' : ''}`} />
              <span><small>TRẠNG THÁI KẾT NỐI</small><strong>{deviceId && !error ? 'Đang theo dõi' : 'Chưa kết nối'}</strong></span>
            </div>
            <div className="overview-strip__divider" />
            <div className="overview-strip__item overview-strip__item--updated">
              <span className="overview-icon overview-icon--sand"><ClockIcon /></span>
              <span><small>CẬP NHẬT LẦN CUỐI</small><strong>{lastUpdated ? lastUpdated.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' }) : '—'}</strong></span>
            </div>
            <button
              className={`refresh-button ${isLoading ? 'refresh-button--loading' : ''}`}
              onClick={() => loadDashboard()}
              disabled={isLoading || !deviceId}
              aria-label="Làm mới dữ liệu"
            >
              <RefreshIcon /> Làm mới
            </button>
          </section>

          <section className="metric-grid" aria-label="Chỉ số cảm biến mới nhất">
            {environmentalStats.map((stat) => (
              <article className="metric-card" key={stat.label}>
                <div className="metric-card__top">
                  <span className={`metric-icon metric-icon--${stat.color}`}>{stat.icon}</span>
                  <span className="metric-card__period">HIỆN TẠI</span>
                </div>
                <span className="metric-card__label">{stat.label}</span>
                <strong className="metric-card__value">
                  {stat.value === null || stat.value === undefined ? '—' : formatValue(stat.value)}
                  <small>{stat.value === null || stat.value === undefined ? '' : stat.unit}</small>
                </strong>
                <span className="metric-card__foot"><span className="metric-live-dot" /> Cập nhật mỗi 30 giây</span>
              </article>
            ))}
            <article className="metric-card metric-card--water">
              <div className="metric-card__top">
                <span className="metric-icon metric-icon--cyan"><DropIcon /></span>
                <span className="metric-card__period">HIỆN TẠI</span>
              </div>
              <span className="metric-card__label">Mực nước bể</span>
              <strong className="metric-card__value">
                {latestWater ? formatValue(latestWater.water) : '—'}
                <small>{latestWater ? 'cm' : ''}</small>
              </strong>
              <span className="metric-card__foot"><span className="metric-live-dot" /> Cập nhật mỗi 30 giây</span>
            </article>
          </section>

          {error && (
            <div className="dashboard-alert" role="alert">
              <WarningIcon />
              <div><strong>Chưa tải được dữ liệu</strong><p>{error}</p></div>
              <button onClick={() => loadDashboard()} disabled={isLoading}>Thử lại</button>
            </div>
          )}

          {!deviceId && (
            <div className="dashboard-empty">
              <span className="dashboard-empty__icon"><DeviceIcon /></span>
              <strong>Bắt đầu theo dõi khu vườn</strong>
              <p>Nhập mã thiết bị ở phía trên để xem dữ liệu cảm biến và biểu đồ thống kê.</p>
            </div>
          )}

          <section className="charts-grid" aria-label="Biểu đồ thống kê">
            <ChartCard
              title="Môi trường khu vườn"
              subtitle="Biến động các chỉ số cảm biến theo thời gian"
              icon={<ChartIcon />}
              range={range}
              onRangeChange={setRange}
              dataCount={environmental.length}
            >
              <LineChart
                data={environmental}
                series={[
                  { key: 'soil', label: 'Độ ẩm đất', unit: '%', color: '#398c68' },
                  { key: 'humidity', label: 'Độ ẩm không khí', unit: '%', color: '#6b9bd2' },
                  { key: 'temperature', label: 'Nhiệt độ', unit: '°C', color: '#df9b58' },
                ]}
                emptyMessage={deviceId ? 'Chưa có dữ liệu môi trường trong khoảng thời gian này.' : 'Chọn thiết bị để xem biểu đồ.'}
              />
            </ChartCard>
            <ChartCard
              title="Mực nước trong bể"
              subtitle="Theo dõi lượng nước khả dụng theo thời gian"
              icon={<DropIcon />}
              range={range}
              onRangeChange={setRange}
              dataCount={water.length}
              water
            >
              <LineChart
                data={water}
                series={[{ key: 'water', label: 'Mực nước', unit: 'cm', color: '#5a9db6' }]}
                emptyMessage={deviceId ? 'Chưa có dữ liệu mực nước trong khoảng thời gian này.' : 'Chọn thiết bị để xem biểu đồ.'}
              />
            </ChartCard>
          </section>

          <footer className="dashboard-footer">
            <span>© 2026 Gardenia · Hệ thống quản lý tưới thông minh</span>
            <span><span className="status-dot" /> Dữ liệu tự làm mới mỗi 30 giây</span>
          </footer>
            </>
          )}
        </div>
      </main>
    </div>
  );
}

function ChartCard({ title, subtitle, icon, range, onRangeChange, dataCount, water, children }) {
  return (
    <article className={`chart-card ${water ? 'chart-card--water' : ''}`}>
      <div className="chart-card__header">
        <div className="chart-card__title-wrap">
          <span className={`chart-card__icon ${water ? 'chart-card__icon--water' : ''}`}>{icon}</span>
          <div><h2>{title}</h2><p>{subtitle}</p></div>
        </div>
        <label className="range-select">
          <span className="sr-only">Khoảng thời gian biểu đồ</span>
          <select value={range} onChange={(event) => onRangeChange(event.target.value)}>
            {RANGE_OPTIONS.map((option) => <option value={option.value} key={option.value}>{option.label}</option>)}
          </select>
          <ChevronIcon />
        </label>
      </div>
      {children}
      <div className="chart-card__bottom">
        <span>{dataCount} điểm dữ liệu</span>
        <span><span className="chart-live-dot" /> Đang cập nhật</span>
      </div>
    </article>
  );
}

function LineChart({ data, series, emptyMessage }) {
  const width = 680;
  const height = 236;
  const left = 38;
  const right = 18;
  const top = 17;
  const bottom = 35;
  const plotWidth = width - left - right;
  const plotHeight = height - top - bottom;
  const visibleData = data.slice(-24);
  const lines = series.map((item) => {
    const values = visibleData.map((row) => row[item.key]).filter((value) => value !== null);
    if (!values.length) return { ...item, points: [] };
    const min = Math.min(0, ...values);
    const max = Math.max(...values);
    const span = Math.max(max - min, 1) * 1.2;
    const lower = min - span * 0.05;
    const upper = max + span * 0.15;
    const points = visibleData
      .map((row, index) => {
        const value = row[item.key];
        if (value === null) return null;
        return {
          x: left + (visibleData.length <= 1 ? plotWidth / 2 : (index / (visibleData.length - 1)) * plotWidth),
          y: top + ((upper - value) / (upper - lower)) * plotHeight,
          value,
          label: row.label,
        };
      })
      .filter(Boolean);
    return { ...item, points };
  });
  const hasData = lines.some((line) => line.points.length);

  return (
    <div className="chart-visual">
      {hasData ? (
        <>
          <div className="chart-legend">
            {lines.filter((line) => line.points.length).map((line) => (
              <span key={line.key}><i style={{ backgroundColor: line.color }} />{line.label}</span>
            ))}
          </div>
          <svg className="line-chart" viewBox={`0 0 ${width} ${height}`} role="img" aria-label={series.map((item) => item.label).join(', ')}>
            {[0, 1, 2, 3, 4].map((gridLine) => {
              const y = top + (gridLine / 4) * plotHeight;
              return <line className="chart-grid-line" key={gridLine} x1={left} x2={width - right} y1={y} y2={y} />;
            })}
            {lines.map((line) => {
              if (!line.points.length) return null;
              const path = line.points.map((point, index) => `${index ? 'L' : 'M'} ${point.x} ${point.y}`).join(' ');
              const areaPath = `${path} L ${line.points[line.points.length - 1].x} ${top + plotHeight} L ${line.points[0].x} ${top + plotHeight} Z`;
              return (
                <g key={line.key}>
                  {line.points.length > 1 && <path d={areaPath} fill={line.color} opacity=".07" />}
                  <path d={path} fill="none" stroke={line.color} strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round" />
                  {line.points.map((point, index) => (
                    <circle key={`${line.key}-${index}`} cx={point.x} cy={point.y} r="3.2" fill="#fff" stroke={line.color} strokeWidth="2">
                      <title>{`${line.label}: ${formatValue(point.value)} ${line.unit} · ${point.label}`}</title>
                    </circle>
                  ))}
                </g>
              );
            })}
            {[0, Math.floor((visibleData.length - 1) / 2), visibleData.length - 1]
              .filter((index, array) => array.indexOf(index) === index)
              .map((index) => {
                if (index < 0) return null;
                const x = left + (visibleData.length <= 1 ? plotWidth / 2 : (index / (visibleData.length - 1)) * plotWidth);
                return <text className="chart-axis-label" key={`label-${index}`} x={x} y={height - 8} textAnchor={index === 0 ? 'start' : index === visibleData.length - 1 ? 'end' : 'middle'}>{formatTimeLabel(visibleData[index]?.label)}</text>;
              })}
          </svg>
        </>
      ) : (
        <div className="chart-empty">
          <span className="chart-empty__icon"><ChartIcon /></span>
          <p>{emptyMessage}</p>
        </div>
      )}
    </div>
  );
}

function formatValue(value) {
  return new Intl.NumberFormat('vi-VN', { maximumFractionDigits: 1 }).format(value);
}

function formatTimeLabel(value) {
  if (!value) return '';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return String(value).slice(-8);
  return new Intl.DateTimeFormat('vi-VN', {
    day: '2-digit',
    month: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
  }).format(date);
}

function LeafIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M20.4 3.6C11.2 3.4 5.1 5 4.1 10.2c-.6 3.1 1.6 5.3 4.6 4.7 5.2-1 6.8-7.1 6.6-11.3M3.8 20.2c2.3-5 5.2-7.8 9.5-10.3" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function GridIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="3" y="3" width="7" height="7" rx="2" stroke="currentColor" strokeWidth="1.7" /><rect x="14" y="3" width="7" height="7" rx="2" stroke="currentColor" strokeWidth="1.7" /><rect x="14" y="14" width="7" height="7" rx="2" stroke="currentColor" strokeWidth="1.7" /><rect x="3" y="14" width="7" height="7" rx="2" stroke="currentColor" strokeWidth="1.7" /></svg>; }
function DeviceIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="4" stroke="currentColor" strokeWidth="1.7" /><path d="M9 9h6v6H9zM12 2v2m0 16v2M2 12h2m16 0h2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function UsersIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M16 20v-1.6a3.4 3.4 0 0 0-3.4-3.4H6.4A3.4 3.4 0 0 0 3 18.4V20m6.5-9a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Zm6-6.7a3.5 3.5 0 0 1 0 6.8m2 3.9h.1a3.4 3.4 0 0 1 3.4 3.4V20" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function PlantIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 20v-8m0 2c-4.8 0-7-2.3-7-7 4.8 0 7 2.3 7 7Zm0-3c0-4.8 2.3-7 7-7 0 4.8-2.3 7-7 7Z" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /><path d="M7 20h10" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>; }
function SoilIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M4 18h16M5 15c2-2 4-2 7 0s5 2 7 0M6 11c2-2 4-2 6 0s4 2 6 0" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /><path d="M12 10V5m0 2c-2.3 0-3.5-1.1-3.5-3.5C10.9 3.5 12 4.7 12 7Zm0-1c0-2.3 1.1-3.5 3.5-3.5C15.5 4.9 14.3 6 12 6Z" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function HumidityIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3.2S5.7 10.5 5.7 14.8a6.3 6.3 0 1 0 12.6 0C18.3 10.5 12 3.2 12 3.2Z" stroke="currentColor" strokeWidth="1.7" strokeLinejoin="round" /><path d="M9.2 15.1a2.8 2.8 0 0 0 2.8 2.8" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>; }
function TemperatureIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M14 14.8V5a3 3 0 0 0-6 0v9.8a5 5 0 1 0 6 0Z" stroke="currentColor" strokeWidth="1.7" strokeLinejoin="round" /><path d="M11 7v10m0 0a2 2 0 1 0 2 2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>; }
function DropIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3.2S5.7 10.5 5.7 14.8a6.3 6.3 0 1 0 12.6 0C18.3 10.5 12 3.2 12 3.2Z" stroke="currentColor" strokeWidth="1.8" strokeLinejoin="round" /><path d="M9.1 15.2a2.9 2.9 0 0 0 2.9 2.9" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" /></svg>; }
function ClockIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><circle cx="12" cy="12" r="8.5" stroke="currentColor" strokeWidth="1.7" /><path d="M12 7v5l3.2 2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function ChartIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M4 19V5m0 14h16M7 15l4-4 3 2 5-6" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" /><circle cx="19" cy="7" r="1.5" fill="currentColor" /></svg>; }
function HelpIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><circle cx="12" cy="12" r="9" stroke="currentColor" strokeWidth="1.7" /><path d="M9.6 9a2.5 2.5 0 1 1 4.6 1.4c-.8 1.1-2.2 1.3-2.2 3m0 3h.01" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>; }
function LogoutIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M10 5H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h4m4-3 4-4-4-4m4 4H9" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function BellIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M18 9a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9m-8 12h4" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function RefreshIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M20 7v5h-5M4 17v-5h5" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /><path d="M5.5 9a7 7 0 0 1 11.6-2.6L20 12M4 12l2.9 5.6A7 7 0 0 0 18.5 15" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function WarningIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3 2.8 19h18.4L12 3Z" stroke="currentColor" strokeWidth="1.7" strokeLinejoin="round" /><path d="M12 9v4m0 3h.01" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" /></svg>; }
function ChevronIcon() { return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="m6 8 4 4 4-4" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function ArrowIcon() { return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M4 10h12m-5-5 5 5-5 5" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function ArrowRightIcon() { return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M4 10h12m-5-5 5 5-5 5" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }

export default Home;
