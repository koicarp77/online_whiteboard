#!/usr/bin/env python3
import os
import signal
import subprocess
import time

def list_cpp_files(root):
    result = []
    for base, _, files in os.walk(root):
        for name in files:
            if name.endswith(".cpp"):
                result.append(os.path.join(base, name))
    return result

def latest_mtime(paths):
    latest = 0
    for path in paths:
        try:
            mtime = os.path.getmtime(path)
            if mtime > latest:
                latest = mtime
        except FileNotFoundError:
            continue
    return latest

def run_server():
    os.makedirs("build", exist_ok=True)
    build = subprocess.run(
        [
            "g++",
            "-o",
            "build/server",
            "src/main.cpp",
            "src/WebSocketServer.cpp",
            "-std=c++17",
            "-lpthread",
            "-lcurl",
            "-lboost_system",
            "-lhiredis",
            "-lcrypto",
            "-I/app/include",
            "-I/app/third_party",
            "-I/app/third_party/jwt-cpp/include",
        ],
        check=False,
    )
    if build.returncode != 0:
        return None
    return subprocess.Popen(["./build/server"], preexec_fn=os.setsid)

def stop_server(proc):
    if proc is None or proc.poll() is not None:
        return
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        return

def main():
    root = "/app"
    files = list_cpp_files(root)
    last_mtime = latest_mtime(files)
    proc = run_server()

    while True:
        time.sleep(1)
        files = list_cpp_files(root)
        current_mtime = latest_mtime(files)
        if current_mtime > last_mtime:
            last_mtime = current_mtime
            stop_server(proc)
            proc = run_server()

if __name__ == "__main__":
    main()
