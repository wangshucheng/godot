"""start_cdn.py - 启动本地 CDN 服务器（后台守护进程）

用法:
    python start_cdn.py            # 前台启动
    python start_cdn.py --daemon   # 后台启动（推荐）
    python start_cdn.py --kill     # 停止
"""
import argparse
import http.server
import os
import socketserver
import sys
import threading
import time

CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
PORT = 8000
PID_FILE = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\.cdn_pid"


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=CDN_ROOT, **kwargs)

    def log_message(self, format, *args):
        # 静默日志
        pass

    def end_headers(self):
        # 禁用缓存，确保改完 CDN 内容立刻生效
        self.send_header("Cache-Control", "no-store, no-cache, must-revalidate")
        self.send_header("Pragma", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        super().end_headers()


def is_port_in_use(port):
    import socket
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(("127.0.0.1", port)) == 0


def kill_existing():
    if os.path.exists(PID_FILE):
        try:
            with open(PID_FILE, "r") as f:
                old_pid = int(f.read().strip())
            import psutil
            try:
                p = psutil.Process(old_pid)
                p.terminate()
                p.wait(timeout=3)
                print(f"Killed old CDN server (pid={old_pid})")
            except (psutil.NoSuchProcess, psutil.TimeoutExpired):
                pass
            os.remove(PID_FILE)
        except Exception:
            pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--daemon", action="store_true", help="Run as background daemon")
    parser.add_argument("--kill", action="store_true", help="Kill existing server")
    args = parser.parse_args()

    if args.kill:
        kill_existing()
        return 0

    if not os.path.isdir(CDN_ROOT):
        print(f"ERROR: CDN root not found: {CDN_ROOT}")
        return 1

    if is_port_in_use(PORT):
        print(f"WARNING: Port {PORT} already in use, killing old server...")
        kill_existing()
        # 也尝试通过 psutil 找占用的进程
        try:
            import psutil
            for conn in psutil.net_connections(kind="inet"):
                if conn.laddr.port == PORT and conn.status == "LISTEN":
                    try:
                        psutil.Process(conn.pid).kill()
                    except Exception:
                        pass
        except ImportError:
            pass
        time.sleep(2)

    if args.daemon:
        # 后台启动
        import subprocess
        py = sys.executable
        me = os.path.abspath(__file__)
        proc = subprocess.Popen(
            [py, me],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            stdin=subprocess.DEVNULL,
            creationflags=subprocess.DETACHED_PROCESS | subprocess.CREATE_NEW_PROCESS_GROUP,
            close_fds=True,
        )
        with open(PID_FILE, "w") as f:
            f.write(str(proc.pid))
        # 等待端口起来
        for _ in range(20):
            time.sleep(0.5)
            if is_port_in_use(PORT):
                break
        if is_port_in_use(PORT):
            print(f"[OK] CDN server running on port {PORT} (pid={proc.pid}, background)")
            print(f"     Serving: {CDN_ROOT}")
            return 0
        else:
            print(f"[ERR] CDN server failed to start")
            return 1
    else:
        # 前台启动
        socketserver.TCPServer.allow_reuse_address = True
        httpd = socketserver.TCPServer(("0.0.0.0", PORT), Handler)
        with open(PID_FILE, "w") as f:
            f.write(str(os.getpid()))
        print(f"[OK] CDN server running on port {PORT} (pid={os.getpid()}, foreground)")
        print(f"     Serving: {CDN_ROOT}")
        print(f"     Press Ctrl+C to stop")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nStopping...")
        finally:
            try:
                os.remove(PID_FILE)
            except Exception:
                pass


if __name__ == "__main__":
    sys.exit(main())
