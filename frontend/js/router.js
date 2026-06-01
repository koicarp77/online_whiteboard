/**
 * 简易前端路由 —— 基于 hash 的 SPA 视图切换。
 */
const routes = {};
let currentView = null;

/**
 * 注册路由。
 * @param {string}   name     路由名称（hash 值，如 'login', 'rooms', 'whiteboard'）
 * @param {function} renderFn 渲染函数 (params) => void
 */
export function register(name, renderFn) {
  routes[name] = renderFn;
}

/** 导航到指定路由 */
export function navigate(name, params = {}) {
  if (name === currentView) return; // 同一视图不重复渲染
  currentView = name;
  const query = new URLSearchParams(params).toString();
  window.location.hash = query ? `#${name}?${query}` : `#${name}`;
}

/** 监听 hash 变化并触发对应路由 */
export function start() {
  window.addEventListener('hashchange', onHashChange);
  onHashChange(); // 首次加载
}

function onHashChange() {
  const hash = window.location.hash.slice(1) || 'login';
  const [name, queryStr] = hash.split('?');
  const params = {};
  if (queryStr) {
    new URLSearchParams(queryStr).forEach((v, k) => { params[k] = v; });
  }
  currentView = name;
  const renderFn = routes[name];
  if (renderFn) {
    renderFn(params);
  } else {
    // 默认跳转登录
    if (routes['login']) routes['login']({});
  }
}
