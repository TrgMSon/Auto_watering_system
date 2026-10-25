import { useState } from 'react';

const API_BASE_URL = process.env.REACT_APP_API_BASE_URL || '';

function getErrorMessage(responseBody, status) {
  if (responseBody?.message) return responseBody.message;
  if (status === 401 || status === 403) return 'Email hoặc mật khẩu không chính xác.';
  return 'Không thể đăng nhập lúc này. Vui lòng thử lại.';
}

function Login({ onLogin }) {
  const [email, setEmail] = useState('');
  const [password, setPassword] = useState('');
  const [showPassword, setShowPassword] = useState(false);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [error, setError] = useState('');

  async function handleSubmit(event) {
    event.preventDefault();
    setError('');
    setIsSubmitting(true);

    try {
      const response = await fetch(`${API_BASE_URL}/api/auth/login`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ email: email.trim(), password }),
      });
      const body = await response.json();

      if (!response.ok || !body.success || !body.data?.accessToken) {
        setError(getErrorMessage(body, response.status));
        return;
      }

      if (body.data.role?.toUpperCase() !== 'ADMIN') {
        setError('Tài khoản này không có quyền quản trị viên.');
        return;
      }

      onLogin(body.data);
    } catch {
      setError('Không thể kết nối đến máy chủ. Hãy kiểm tra backend rồi thử lại.');
    } finally {
      setIsSubmitting(false);
    }
  }

  return (
    <main className="login-page">
      <section className="login-showcase" aria-label="Giới thiệu Gardenia">
        <div className="login-showcase__top">
          <a className="brand brand--light" href="/login" aria-label="Gardenia">
            <span className="brand__mark"><LeafIcon /></span>
            <span>gardenia<span className="brand__dot">.</span></span>
          </a>
          <span className="showcase-pill"><span className="status-dot" /> HỆ THỐNG ĐANG HOẠT ĐỘNG</span>
        </div>

        <div className="showcase-copy">
          <span className="eyebrow eyebrow--light">NÔNG NGHIỆP THÔNG MINH</span>
          <h1>Chăm sóc khu vườn, <em>theo cách thông minh hơn.</em></h1>
          <p>Theo dõi sức khỏe cây trồng và quản lý hệ thống tưới của bạn trong một không gian duy nhất.</p>
        </div>

        <div className="showcase-visual" aria-hidden="true">
          <div className="showcase-visual__sun" />
          <div className="showcase-visual__hill showcase-visual__hill--back" />
          <div className="showcase-visual__hill showcase-visual__hill--front" />
          <div className="showcase-plant showcase-plant--one"><i /><i /><b /></div>
          <div className="showcase-plant showcase-plant--two"><i /><i /><b /></div>
          <div className="showcase-plant showcase-plant--three"><i /><i /><b /></div>
          <div className="showcase-metric">
            <span className="showcase-metric__icon"><DropIcon /></span>
            <span><strong>Đất đang đủ ẩm</strong><small>Độ ẩm đất <b>64%</b></small></span>
            <span className="showcase-metric__check">✓</span>
          </div>
          <div className="showcase-weather"><SunIcon /><span>28°</span><small>Trời nắng nhẹ</small></div>
        </div>

        <div className="showcase-footer">
          <span>ĐƯỢC TẠO CHO NHỮNG KHU VƯỜN TỐT HƠN</span>
          <span>© 2026 GARDENIA</span>
        </div>
      </section>

      <section className="login-panel">
        <div className="login-panel__top">
          <span className="login-panel__hint">Chưa có quyền truy cập?</span>
          <a href="mailto:admin@gardenia.local">Liên hệ quản trị viên <ArrowIcon /></a>
        </div>

        <div className="login-card">
          <span className="eyebrow">CỔNG QUẢN TRỊ</span>
          <h2>Chào mừng trở lại<span className="brand__dot">.</span></h2>
          <p className="login-card__subtitle">Đăng nhập để tiếp tục quản lý khu vườn của bạn.</p>

          <form onSubmit={handleSubmit} className="login-form">
            <label htmlFor="login-email">Địa chỉ email</label>
            <div className="input-wrap">
              <MailIcon />
              <input
                id="login-email"
                type="email"
                autoComplete="username"
                placeholder="name@example.com"
                value={email}
                onChange={(event) => setEmail(event.target.value)}
                required
              />
            </div>

            <div className="form-label-row">
              <label htmlFor="login-password">Mật khẩu</label>
              <button
                className="text-button"
                type="button"
                onClick={() => setShowPassword((visible) => !visible)}
                aria-label={showPassword ? 'Ẩn mật khẩu' : 'Hiện mật khẩu'}
              >
                {showPassword ? 'Ẩn' : 'Hiện'}
              </button>
            </div>
            <div className="input-wrap">
              <LockIcon />
              <input
                id="login-password"
                type={showPassword ? 'text' : 'password'}
                autoComplete="current-password"
                placeholder="Nhập mật khẩu của bạn"
                value={password}
                onChange={(event) => setPassword(event.target.value)}
                required
              />
            </div>

            {error && <div className="form-error" role="alert">{error}</div>}

            <button className="button button--primary login-submit" type="submit" disabled={isSubmitting}>
              {isSubmitting ? <><span className="button-spinner" /> Đang xác thực...</> : <>Đăng nhập <ArrowRightIcon /></>}
            </button>
          </form>

          <div className="login-security"><ShieldIcon /> Kết nối của bạn được bảo mật và mã hóa.</div>
        </div>

        <div className="login-panel__footer">
          <span>© 2026 Gardenia. All rights reserved.</span>
          <a href="mailto:support@gardenia.local">Trợ giúp <ArrowUpRightIcon /></a>
        </div>
      </section>
    </main>
  );
}

function LeafIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M20.4 3.6C11.2 3.4 5.1 5 4.1 10.2c-.6 3.1 1.6 5.3 4.6 4.7 5.2-1 6.8-7.1 6.6-11.3M3.8 20.2c2.3-5 5.2-7.8 9.5-10.3" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}
function DropIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3.2S5.7 10.5 5.7 14.8a6.3 6.3 0 1 0 12.6 0C18.3 10.5 12 3.2 12 3.2Z" stroke="currentColor" strokeWidth="1.8" strokeLinejoin="round" /><path d="M9.1 15.2a2.9 2.9 0 0 0 2.9 2.9" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" /></svg>;
}
function SunIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><circle cx="12" cy="12" r="4" stroke="currentColor" strokeWidth="1.7" /><path d="M12 2v2m0 16v2M4.9 4.9l1.4 1.4m11.4 11.4 1.4 1.4M2 12h2m16 0h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>;
}
function MailIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="3" y="5" width="18" height="14" rx="3" stroke="currentColor" strokeWidth="1.7" /><path d="m4 7 8 6 8-6" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}
function LockIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><rect x="4" y="10" width="16" height="11" rx="3" stroke="currentColor" strokeWidth="1.7" /><path d="M8 10V7a4 4 0 1 1 8 0v3m-4 5v2" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" /></svg>;
}
function ShieldIcon() {
  return <svg viewBox="0 0 24 24" fill="none" aria-hidden="true"><path d="M12 3 20 6v5c0 5.2-3.4 8.4-8 10-4.6-1.6-8-4.8-8-10V6l8-3Z" stroke="currentColor" strokeWidth="1.6" strokeLinejoin="round" /><path d="m9 12 2 2 4-4" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}
function ArrowIcon() {
  return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M4 10h12m-5-5 5 5-5 5" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}
function ArrowRightIcon() {
  return <svg viewBox="0 0 20 20" fill="none" aria-hidden="true"><path d="M4 10h12m-5-5 5 5-5 5" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}
function ArrowUpRightIcon() {
  return <svg viewBox="0 0 18 18" fill="none" aria-hidden="true"><path d="M5 13 13 5M6 5h7v7" stroke="currentColor" strokeWidth="1.4" strokeLinecap="round" strokeLinejoin="round" /></svg>;
}

export default Login;
