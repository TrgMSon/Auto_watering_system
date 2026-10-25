import { useEffect, useState } from 'react';
import './App.css';
import Home from './component/Home';
import Login from './component/Login';

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

  useEffect(() => {
    const handlePopState = () => setPathname(window.location.pathname);
    window.addEventListener('popstate', handlePopState);
    return () => window.removeEventListener('popstate', handlePopState);
  }, []);

  const navigate = (path) => {
    window.history.pushState({}, '', path);
    setPathname(path);
  };

  const handleLogin = (loginData) => {
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

  const handleLogout = () => {
    localStorage.removeItem(AUTH_STORAGE_KEY);
    setAuth(null);
    navigate('/login');
  };

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
