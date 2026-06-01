/**
 * 认证状态管理 —— token 持久化到 localStorage，并提供工具函数。
 */

const STORAGE_KEY = 'whiteboard_auth';

let _auth = null; // 内存缓存

function load() {
  if (_auth) return _auth;
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    _auth = raw ? JSON.parse(raw) : null;
  } catch {
    _auth = null;
  }
  return _auth;
}

function save(auth) {
  _auth = auth;
  if (auth) {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(auth));
  } else {
    localStorage.removeItem(STORAGE_KEY);
  }
}

/** 获取当前 access token */
export function getAccessToken() {
  const auth = load();
  return auth ? auth.access_token : null;
}

/** 获取 refresh token */
export function getRefreshToken() {
  const auth = load();
  return auth ? auth.refresh_token : null;
}

/** 是否已登录 */
export function isLoggedIn() {
  return !!getAccessToken();
}

/** 保存登录凭据 */
export function saveAuth({ access_token, refresh_token, expires_in }) {
  save({ access_token, refresh_token, expires_in });
}

/** 更新 access token（刷新成功后调用） */
export function updateAccessToken(access_token, expires_in) {
  const auth = load();
  if (auth) {
    auth.access_token = access_token;
    auth.expires_in = expires_in;
    save(auth);
  }
}

/** 清除登录态 */
export function clearAuth() {
  save(null);
}
