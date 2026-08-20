/**
 * 主应用逻辑 —— 组装登录、房间列表、白板三个视图。
 */
import * as api from './api.js';
import * as auth from './auth.js';
import * as wsClient from './ws.js';
import * as whiteboard from './whiteboard.js';
import * as router from './router.js';

// ---- DOM 引用 ----
const appEl = document.getElementById('app');

// ---- 工具函数 ----
function $(sel) { return document.querySelector(sel); }
function $$(sel) { return document.querySelectorAll(sel); }

/** 切换到指定视图（控制 CSS 显示/隐藏） */
function showView(viewName) {
  $$('.view').forEach(el => el.style.display = 'none');
  const view = $(`#view-${viewName}`);
  if (view) view.style.display = 'block';
}

// ===================== 登录视图 =====================
function renderLogin() {
  showView('login');
}

async function handleLogin(e) {
  e.preventDefault();
  const username = $('#login-username').value.trim();
  const password = $('#login-password').value.trim();
  const msgEl = $('#login-msg');

  if (!username || !password) {
    msgEl.textContent = '请输入用户名和密码';
    return;
  }

  try {
    msgEl.textContent = '登录中...';
    const data = await api.login(username, password);
    auth.saveAuth(data);
    msgEl.textContent = '';
    router.navigate('rooms');
  } catch (err) {
    msgEl.textContent = `登录失败: ${err.message}`;
  }
}

// ===================== 房间列表视图 =====================
async function renderRooms() {
  if (!auth.isLoggedIn()) { router.navigate('login'); return; }
  showView('rooms');
  await loadRoomList();
}

async function loadRoomList() {
  const listEl = $('#room-list');
  try {
    listEl.innerHTML = '<tr><td colspan="5" class="loading">加载中...</td></tr>';
    const rooms = await api.listRooms(50, 0);
    if (!rooms || rooms.length === 0) {
      listEl.innerHTML = '<tr><td colspan="5" class="empty">暂无活跃房间，请创建一个</td></tr>';
      return;
    }
    listEl.innerHTML = rooms.map(r => `
      <tr>
        <td title="${escHtml(r.id)}">${escHtml(r.id).slice(0, 8)}...</td>
        <td>${escHtml(r.name)}</td>
        <td>${escHtml(r.description || '-')}</td>
        <td>${escHtml(r.owner_id)}</td>
        <td>
          <button class="btn btn-sm btn-primary" data-room-id="${escHtml(r.id)}">进入</button>
        </td>
      </tr>
    `).join('');

    // 绑定「进入房间」按钮事件
    listEl.querySelectorAll('[data-room-id]').forEach(btn => {
      btn.addEventListener('click', () => {
        const rid = btn.getAttribute('data-room-id');
        router.navigate('whiteboard', { room_id: rid });
      });
    });
  } catch (err) {
    listEl.innerHTML = `<tr><td colspan="5" class="error">加载失败: ${err.message}</td></tr>`;
  }
}

async function handleCreateRoom(e) {
  e.preventDefault();
  const name = $('#room-name').value.trim();
  const desc = $('#room-desc').value.trim();
  const maxUsers = parseInt($('#room-max-users').value) || 100;
  const msgEl = $('#room-msg');

  if (!name) { msgEl.textContent = '请输入房间名称'; return; }

  try {
    msgEl.textContent = '创建中...';
    await api.createRoom({ name, description: desc, max_users: maxUsers });
    msgEl.textContent = '';
    $('#room-name').value = '';
    $('#room-desc').value = '';
    await loadRoomList();
  } catch (err) {
    msgEl.textContent = `创建失败: ${err.message}`;
  }
}

async function handleLogout() {
  try {
    await api.logout(auth.getRefreshToken());
  } catch { /* ignore */ }
  auth.clearAuth();
  router.navigate('login');
}

// ===================== 白板视图 =====================
async function renderWhiteboard(params) {
  if (!auth.isLoggedIn()) { router.navigate('login'); return; }

  const roomId = params.room_id;
  if (!roomId) { router.navigate('rooms'); return; }

  showView('whiteboard');

  try {
    // 1. 调用 joinRoom 获取房间信息和 WS 连接参数
    const joinData = await api.joinRoom(roomId);
    const { room, ws_host, ws_port, ws_proto } = joinData;

    // 显示房间信息
    $('#wb-room-name').textContent = room.name;
    $('#wb-room-id').textContent = room.id;
    $('#wb-room-owner').textContent = room.owner_id;
    $('#wb-room-users').textContent = `${room.max_users} 人上限`;

    // 2. 初始化白板画布
    whiteboard.init('wb-container', roomId);

    // 3. 连接 WebSocket（使用当前页面的 hostname，因为 Docker 内部服务名浏览器无法解析）
    wsClient.connect(
      { host: window.location.hostname, port: ws_port, proto: ws_proto },
      (data) => {
        // 收到消息：根据类型处理
        if (data.type === 'draw') {
          whiteboard.handleRemoteDraw(data);
        } else if (data.type === 'clear') {
          whiteboard.clearCanvas(false); // 不再二次广播
        }
      },
      () => { console.log('[App] WebSocket 已连接，开始协作'); },
      () => { console.log('[App] WebSocket 已断开'); }
    );
  } catch (err) {
    alert(`加入房间失败: ${err.message}`);
    router.navigate('rooms');
  }
}

function handleLeaveRoom() {
  wsClient.disconnect();
  whiteboard.destroy();
  router.navigate('rooms');
}

// ===================== 事件绑定 =====================
function bindEvents() {
  // 登录表单
  $('#login-form').addEventListener('submit', handleLogin);

  // 房间创建表单
  $('#room-form').addEventListener('submit', handleCreateRoom);

  // 退出登录
  $('#btn-logout').addEventListener('click', handleLogout);

  // 离开房间
  $('#btn-leave-room').addEventListener('click', handleLeaveRoom);

  // 白板工具栏
  $('#btn-pen').addEventListener('click', () => whiteboard.setTool('pen'));
  $('#btn-eraser').addEventListener('click', () => whiteboard.setTool('eraser'));
  $('#color-picker').addEventListener('input', (e) => whiteboard.setColor(e.target.value));
  $('#line-width').addEventListener('input', (e) => whiteboard.setLineWidth(e.target.value));
  $('#btn-clear').addEventListener('click', () => whiteboard.clearCanvas());
}

// ===================== 启动应用 =====================
function init() {
  // 注册路由
  router.register('login', renderLogin);
  router.register('rooms', renderRooms);
  router.register('whiteboard', renderWhiteboard);

  bindEvents();
  router.start();
}

// DOM 加载完成后启动
document.addEventListener('DOMContentLoaded', init);

// 工具：HTML 转义
function escHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}
