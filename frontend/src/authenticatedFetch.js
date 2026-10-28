export const AUTH_EXPIRED_EVENT = 'garden-admin:access-token-expired';

export async function authenticatedFetch(input, init, accessToken) {
  const response = await fetch(input, init);
  if (response.status === 401) {
    window.dispatchEvent(new CustomEvent(AUTH_EXPIRED_EVENT, {
      detail: { accessToken },
    }));
  }
  return response;
}

export function getAccessTokenExpiry(accessToken) {
  if (typeof accessToken !== 'string') return null;

  const payload = accessToken.split('.')[1];
  if (!payload) return null;

  try {
    const base64 = payload.replace(/-/g, '+').replace(/_/g, '/');
    const paddedBase64 = base64.padEnd(Math.ceil(base64.length / 4) * 4, '=');
    const claims = JSON.parse(window.atob(paddedBase64));
    return Number.isFinite(claims.exp) ? claims.exp * 1000 : null;
  } catch {
    return null;
  }
}
