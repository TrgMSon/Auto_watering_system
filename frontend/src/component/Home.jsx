import { useState } from 'react';
import DeviceManagement from './DeviceManagement';
import FirmwareManagement from './FirmwareManagement';
import UserManagement from './UserManagement';

function Home({ auth, onLogout }) {
  const [activePage, setActivePage] = useState('devices');
  const userName = auth.user?.fullName || auth.user?.name || auth.user?.email || 'Quản trị viên';
  const initials = userName.trim().charAt(0).toUpperCase() || 'A';
  const pageTitles = {
    devices: 'Thiết bị IoT',
    firmware: 'Cập nhật firmware',
    users: 'Người dùng',
  };

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
            className={`side-nav__item side-nav__button ${activePage === 'devices' ? 'side-nav__item--active' : ''}`}
            onClick={() => setActivePage('devices')}
            aria-current={activePage === 'devices' ? 'page' : undefined}
          >
            <DeviceIcon /> <span>Thiết bị IoT</span>
          </button>
          <button
            className={`side-nav__item side-nav__button ${activePage === 'firmware' ? 'side-nav__item--active' : ''}`}
            onClick={() => setActivePage('firmware')}
            aria-current={activePage === 'firmware' ? 'page' : undefined}
          >
            <FirmwareIcon /> <span>Firmware OTA</span>
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
            <strong>{pageTitles[activePage]}</strong>
          </div>
          <div className="topbar__actions">
            <div className="system-status"><span className="status-dot" /> Hệ thống hoạt động</div>
            <span className="topbar-avatar">{initials}</span>
          </div>
        </header>

        <div className="dashboard-content">
          {activePage === 'devices' && <DeviceManagement accessToken={auth.accessToken} />}
          {activePage === 'firmware' && <FirmwareManagement accessToken={auth.accessToken} />}
          {activePage === 'users' && <UserManagement accessToken={auth.accessToken} />}
        </div>
      </main>
    </div>
  );
}

function LeafIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M20.4 3.6C11.2 3.4 5.1 5 4.1 10.2c-.6 3.1 1.6 5.3 4.6 4.7 5.2-1 6.8-7.1 6.6-11.3M3.8 20.2c2.3-5 5.2-7.8 9.5-10.3" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function DeviceIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="4" y="4" width="16" height="16" rx="4" stroke="currentColor" strokeWidth="1.7" /><path d="M9 9h6v6H9zM12 2v2m0 16v2M2 12h2m16 0h2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function FirmwareIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 16V4m0 0L7.5 8.5M12 4l4.5 4.5M5 14v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-4" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function UsersIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M16 20v-1.6a3.4 3.4 0 0 0-3.4-3.4H6.4A3.4 3.4 0 0 0 3 18.4V20m6.5-9a3.5 3.5 0 1 0 0-7 3.5 3.5 0 0 0 0 7Zm6-6.7a3.5 3.5 0 0 1 0 6.8m2 3.9h.1a3.4 3.4 0 0 1 3.4 3.4V20" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function HelpIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><circle cx="12" cy="12" r="9" stroke="currentColor" strokeWidth="1.7" /><path d="M9.6 9a2.5 2.5 0 1 1 4.6 1.4c-.8 1.1-2.2 1.3-2.2 3m0 3h.01" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>; }
function LogoutIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M10 5H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h4m4-3 4-4-4-4m4 4H9" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function BellIcon() { return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M18 9a6 6 0 0 0-12 0c0 7-3 7-3 9h18c0-2-3-2-3-9m-8 12h4" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>; }
function ArrowIcon() { return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M4 10h12m-5-5 5 5-5 5" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>; }

export default Home;
