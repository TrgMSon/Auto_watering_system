import { useCallback, useEffect, useRef, useState } from 'react';
import './App.css';
import Home from './component/Home';
import Login from './component/Login';
import { AUTH_EXPIRED_EVENT, getAccessTokenExpiry } from './authenticatedFetch';

const AUTH_STORAGE_KEY = 'garden-admin-auth';

function readAuth() {
  try {
    const savedAuth = localStorage.getItem(AUTH_STORAGE_KEY);
    return savedAuth ? JSON.parse(savedAuth) : null;
  } catch {
    localStorage.removeItem(AUTH_STORAGE_KEY);
    return null;
  }
}

function App() {
  const [auth, setAuth] = useState(readAuth);
  const [pathname, setPathname] = useState(window.location.pathname);
  const logoutStarted = useRef(false);

  useEffect(() => {
    const handlePopState = () => setPathname(window.location.pathname);
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  const navigate = useCallback((path) => {
    window.history.pushState({}, '', path);
    setPathname(path);
  }, []);

  const handleLogin = (loginData) => {
    logoutStarted.current = false;
    const nextAuth = {
      accessToken: loginData.accessToken,
      refreshToken: loginData.refreshToken,
      role: loginData.role,
      user: loginData.userResponse,
    };
    localStorage.setItem(AUTH_STORAGE_KEY, JSON.stringify(nextAuth));
    setAuth(nextAuth);
    navigate('/');
  };

  const handleLogout = useCallback(() => {
    if (logoutStarted.current) return;
    logoutStarted.current = true;
    localStorage.removeItem(AUTH_STORAGE_KEY);
    setAuth(null);
    navigate('/login');
  }, [navigate]);

  useEffect(() => {
    if (!auth?.accessToken) return undefined;

    const handleExpiredEvent = (event) => {
      if (event.detail?.accessToken === auth.accessToken) {
        handleLogout();
      }
    };
    window.addEventListener(AUTH_EXPIRED_EVENT, handleExpiredEvent);
    return () => window.removeEventListener(AUTH_EXPIRED_EVENT, handleExpiredEvent);
  }, [auth?.accessToken, handleLogout]);

  useEffect(() => {
    const expiresAt = getAccessTokenExpiry(auth?.accessToken);
    if (expiresAt === null) return undefined;

    let timeoutId;
    const checkExpiration = () => {
      const remainingTime = expiresAt - Date.now();
      if (remainingTime <= 0) {
        handleLogout();
        return;
      }
      timeoutId = window.setTimeout(
        checkExpiration,
        Math.min(remainingTime, 2_147_483_647),
      );
    };

    checkExpiration();
    return () => window.clearTimeout(timeoutId);
  }, [auth?.accessToken, handleLogout]);

  if (pathname === '/login' || !auth?.accessToken) {
    return <Login onLogin={handleLogin} />;
  }

  if (auth.role?.toUpperCase() !== 'ADMIN') {
    return (
      <main className="access-denied">
        <div className="access-denied__card">
          <span className="access-denied__icon" aria-hidden="true">!</span>
          <h1>Không có quyền truy cập</h1>
          <p>Khu vực này chỉ dành cho tài khoản quản trị viên.</p>
          <button className="button button--primary" onClick={handleLogout}>
            Đăng xuất
          </button>
        </div>
      </main>
    );
  }

  return <Home auth={auth} onLogout={handleLogout} />;
}

export default App;
