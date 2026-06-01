/**
 * WebSocket 客户端 —— 连接到 C++ 实时协作服务器。
 */
import { getAccessToken } from './auth.js';

let ws = null;
let messageHandler = null;
let reconnectTimer = null;
let wsUrl = null;

/**
 * 建立 WebSocket 连接。
 * @param {string} host  WebSocket 主机
 * @param {number} port  WebSocket 端口
 * @param {string} proto ws 或 wss
 * @param {function} onMessage 收到消息的回调 (data) => void
 * @param {function} onOpen   连接成功回调
 * @param {function} onClose  连接关闭回调
 */
export function connect({ host, port, proto }, onMessage, onOpen, onClose) {
  const token = getAccessToken();
  if (!token) {
    console.error('[WS] 缺少 access token，无法连接');
    return;
  }

  wsUrl = `${proto}://${host}:${port}/?token=${encodeURIComponent(token)}`;
  ws = new WebSocket(wsUrl);

  ws.onopen = () => {
    console.log('[WS] 连接成功:', wsUrl);
    if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
    if (onOpen) onOpen();
  };

  ws.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      if (onMessage) onMessage(data);
    } catch {
      if (onMessage) onMessage(event.data);
    }
  };

  ws.onclose = (event) => {
    console.warn('[WS] 连接关闭:', event.code, event.reason);
    if (onClose) onClose(event);
    // 自动重连（3 秒后）
    if (event.code !== 1000 && !reconnectTimer) {
      reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        if (host && port && proto) {
          console.log('[WS] 正在重连...');
          connect({ host, port, proto }, onMessage, onOpen, onClose);
        }
      }, 3000);
    }
  };

  ws.onerror = (err) => {
    console.error('[WS] 连接错误:', err);
  };
}

/** 发送消息到服务器 */
export function send(data) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(typeof data === 'string' ? data : JSON.stringify(data));
  } else {
    console.warn('[WS] 连接未就绪，消息未发送');
  }
}

/** 断开连接 */
export function disconnect() {
  if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
  if (ws) {
    ws.close(1000, 'User disconnected');
    ws = null;
  }
}

/** 连接是否就绪 */
export function isConnected() {
  return ws && ws.readyState === WebSocket.OPEN;
}
