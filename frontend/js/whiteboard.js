/**
 * 白板画布 —— 处理本地绘图 + 同步远端笔画。
 */
import * as wsClient from './ws.js';

const PEN = 'pen';
const ERASER = 'eraser';
const SHAPES = { PEN, ERASER };

// 笔画消息类型
const MSG_DRAW = 'draw';
const MSG_CLEAR = 'clear';

let canvas, ctx;
let isDrawing = false;
let currentTool = PEN;
let currentColor = '#000000';
let currentLineWidth = 3;
let roomId = '';

// 用于 Undo（本地简化版）
let undoStack = [];

/**
 * 初始化白板到指定容器。
 * @param {string} containerId  容器 DOM id
 * @param {string} roomIdValue  当前房间 ID
 */
export function init(containerId, roomIdValue) {
  roomId = roomIdValue;

  // 若已存在则先移除
  const old = document.getElementById('wb-canvas');
  if (old) old.remove();

  const container = document.getElementById(containerId);
  if (!container) return;

  canvas = document.createElement('canvas');
  canvas.id = 'wb-canvas';
  canvas.width = container.clientWidth;
  canvas.height = container.clientHeight;
  canvas.style.cssText = 'display:block;cursor:crosshair;';
  container.appendChild(canvas);

  ctx = canvas.getContext('2d');
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';

  bindCanvasEvents();

  // 窗口大小变化时自适应
  window.addEventListener('resize', handleResize);
}

/** 画布尺寸自适应 */
function handleResize() {
  if (!canvas) return;
  const parent = canvas.parentElement;
  if (!parent) return;
  const data = canvas.toDataURL();
  canvas.width = parent.clientWidth;
  canvas.height = parent.clientHeight;
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  const img = new Image();
  img.onload = () => ctx.drawImage(img, 0, 0);
  img.src = data;
}

/** 绑定鼠标 / 触摸事件 */
function bindCanvasEvents() {
  // 鼠标
  canvas.addEventListener('mousedown', startDraw);
  canvas.addEventListener('mousemove', moveDraw);
  canvas.addEventListener('mouseup', endDraw);
  canvas.addEventListener('mouseleave', endDraw);

  // 触摸
  canvas.addEventListener('touchstart', (e) => { e.preventDefault(); startDraw(e.touches[0]); });
  canvas.addEventListener('touchmove', (e) => { e.preventDefault(); moveDraw(e.touches[0]); });
  canvas.addEventListener('touchend', endDraw);
}

function getPos(e) {
  const rect = canvas.getBoundingClientRect();
  return {
    x: (e.clientX - rect.left) * (canvas.width / rect.width),
    y: (e.clientY - rect.top) * (canvas.height / rect.height),
  };
}

function startDraw(e) {
  isDrawing = true;
  const pos = getPos(e);
  ctx.beginPath();
  ctx.moveTo(pos.x, pos.y);
  ctx.strokeStyle = currentTool === ERASER ? '#FFFFFF' : currentColor;
  ctx.lineWidth = currentTool === ERASER ? currentLineWidth * 3 : currentLineWidth;
}

function moveDraw(e) {
  if (!isDrawing) return;
  const pos = getPos(e);
  ctx.lineTo(pos.x, pos.y);
  ctx.stroke();

  // 发送笔画增量到服务器
  wsClient.send({
    type: MSG_DRAW,
    room_id: roomId,
    tool: currentTool,
    color: currentColor,
    lineWidth: currentLineWidth,
    x: pos.x,
    y: pos.y,
  });
}

function endDraw() {
  if (!isDrawing) return;
  isDrawing = false;
  ctx.closePath();
}

/** 处理远端笔画 */
export function handleRemoteDraw(data) {
  if (!ctx) return;
  ctx.save();
  ctx.beginPath();
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  ctx.strokeStyle = data.tool === ERASER ? '#FFFFFF' : data.color;
  ctx.lineWidth = data.tool === ERASER ? data.lineWidth * 3 : data.lineWidth;
  // 远端笔画：从上一坐标画到当前坐标
  if (data.prevX !== undefined && data.prevY !== undefined) {
    ctx.moveTo(data.prevX, data.prevY);
    ctx.lineTo(data.x, data.y);
  } else {
    // 若无历史坐标，简单画一个点
    ctx.moveTo(data.x, data.y);
    ctx.lineTo(data.x + 0.5, data.y + 0.5);
  }
  ctx.stroke();
  ctx.closePath();
  ctx.restore();
}

/** 清空画布 */
export function clearCanvas(sync = true) {
  if (!ctx || !canvas) return;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  if (sync) {
    wsClient.send({ type: MSG_CLEAR, room_id: roomId });
  }
}

// --- 工具设置 ---

export function setTool(tool) {
  currentTool = tool;
  if (canvas) {
    canvas.style.cursor = tool === ERASER ? 'cell' : 'crosshair';
  }
}

export function setColor(color) {
  currentColor = color;
}

export function setLineWidth(width) {
  currentLineWidth = Number(width);
}

export function getCurrentTool() {
  return currentTool;
}

export function getCurrentColor() {
  return currentColor;
}

/** 销毁白板 */
export function destroy() {
  isDrawing = false;
  if (canvas) { canvas.remove(); canvas = null; ctx = null; }
  window.removeEventListener('resize', handleResize);
}
