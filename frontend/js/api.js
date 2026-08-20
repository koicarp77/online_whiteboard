/**
 * HTTP API 客户端 —— 封装对 Go 后端所有接口的请求。
 */
import CONFIG from './config.js';
import { getAccessToken } from './auth.js';

const BASE = CONFIG.API_BASE;

/**
 * 通用请求封装，自动附带 Authorization header 和 JSON 解析。
 */
async function request(method, path, body = null, authenticated = true) {
  const headers = { 'Content-Type': 'application/json' };
  if (authenticated) {
    const token = getAccessToken();
    if (token) headers['Authorization'] = `Bearer ${token}`;
  }

  const opts = { method, headers };
  if (body !== null) opts.body = JSON.stringify(body);

  const resp = await fetch(`${BASE}${path}`, opts);
  const json = await resp.json();

  if (!resp.ok || json.code !== 0) {
    throw new ApiError(json.message || resp.statusText, json.code, resp.status);
  }
  return json.data;
}

export class ApiError extends Error {
  constructor(message, code, httpStatus) {
    super(message);
    this.name = 'ApiError';
    this.code = code;
    this.httpStatus = httpStatus;
  }
}

/* ========== 认证接口 ========== */

/** 登录，返回 { access_token, refresh_token, expires_in } */
export async function login(username, password) {
  return request('POST', '/auth/login', { username, password }, false);
}

/** 刷新 access token */
export async function refreshToken(refreshTokenValue) {
  return request('POST', '/auth/refresh', { refresh_token: refreshTokenValue }, false);
}

/** 登出，撤销 refresh token */
export async function logout(refreshTokenValue) {
  return request('POST', '/auth/logout', { refresh_token: refreshTokenValue }, false);
}

/* ========== 房间接口 ========== */

/** 创建房间 */
export async function createRoom({ name, description, max_users }) {
  return request('POST', '/room/create', {
    name,
    description: description || '',
    owner_id: '',       // 后端会从 token 中自动填充
    max_users: max_users || 100,
  });
}

/** 获取活跃房间列表（分页） */
export async function listRooms(limit = 20, offset = 0) {
  return request('GET', `/room/list?limit=${limit}&offset=${offset}`);
}

/** 加入房间，返回 { room, ws_host, ws_port, ws_proto } */
export async function joinRoom(roomId) {
  return request('POST', '/room/join', { room_id: roomId });
}
