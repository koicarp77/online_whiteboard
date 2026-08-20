/**
 * 前端配置 —— 所有环境相关的地址集中在此，方便切换。
 */
const CONFIG = {
  // Go HTTP API 服务地址（通过代理同源访问，避免跨域）
  API_BASE: '',

  // WebSocket 实时协作服务地址（由 joinRoom 接口动态返回，此处仅作 fallback）
  WS_HOST: 'localhost',
  WS_PORT: 8080,
  WS_PROTO: 'ws',
};

export default CONFIG;
