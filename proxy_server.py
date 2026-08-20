"""
前端开发服务器 —— 同时提供静态文件服务和 API 反向代理。
所有请求都走 localhost:3000，彻底避免跨域问题。

用法：python proxy_server.py
- 静态文件：从 ./frontend 目录提供
- API 代理：/auth/* 和 /room/* → localhost:9090
"""

import http.server
import urllib.request
import urllib.error
import os
import sys

PORT = 3000
API_TARGET = "http://localhost:9090"
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "frontend")

# 需要代理到后端 Go 服务的路径前缀
PROXY_PREFIXES = ("/auth/", "/room/")


class ProxyHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def do_GET(self):
        if self._should_proxy():
            self._proxy_request("GET")
        else:
            super().do_GET()

    def do_POST(self):
        if self._should_proxy():
            self._proxy_request("POST")
        else:
            # 默认返回 405
            self.send_error(405, "Method Not Allowed")

    def do_OPTIONS(self):
        """直接响应 CORS 预检请求，不转发到后端。"""
        self.send_response(200)
        self._send_cors_headers()
        self.end_headers()

    def _should_proxy(self):
        return any(self.path.startswith(p) for p in PROXY_PREFIXES)

    def _proxy_request(self, method):
        target_url = API_TARGET + self.path
        try:
            # 读取请求体
            content_length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(content_length) if content_length > 0 else None

            req = urllib.request.Request(target_url, data=body, method=method)
            # 转发相关请求头
            for header in ("Content-Type", "Authorization"):
                val = self.headers.get(header)
                if val:
                    req.add_header(header, val)

            with urllib.request.urlopen(req, timeout=30) as resp:
                self.send_response(resp.status)
                self._send_cors_headers()
                # 转发 Content-Type
                ct = resp.headers.get("Content-Type")
                if ct:
                    self.send_header("Content-Type", ct)
                self.end_headers()
                self.wfile.write(resp.read())

        except urllib.error.HTTPError as e:
            self.send_response(e.code)
            self._send_cors_headers()
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(e.read())

        except Exception as e:
            self.send_response(502)
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(f'{{"code":502,"message":"proxy error: {e}"}}'.encode())

    def _send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")

    def log_message(self, fmt, *args):
        # 增强日志：显示代理目标
        if self._should_proxy():
            print(f"[proxy → {API_TARGET}{self.path}] {args[0]}")
        else:
            super().log_message(fmt, *args)


if __name__ == "__main__":
    os.chdir(STATIC_DIR)
    server = http.server.HTTPServer(("0.0.0.0", PORT), ProxyHandler)
    print(f"[OK] Frontend dev server started")
    print(f"   Static: {STATIC_DIR}")
    print(f"   Proxy:  /auth/*, /room/* -> {API_TARGET}")
    print(f"   URL:    http://localhost:{PORT}")
    print()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped")
        server.server_close()
