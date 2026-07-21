"""run_with_http.py - Open IDE with HTTP port, probe API, trigger compile"""
import subprocess
import time
import os
import sys
import threading
import http.server
import socketserver
import urllib.request
import urllib.error
import json

CLI = r"D:\software\Tencent\微信web开发者工具\cli.bat"
PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
HTTP_PORT = 9911
LOG_DIR = r"C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappLog\logs"


def log(msg):
    ts = time.strftime("%H:%M:%S")
    print(f"[{ts}] {msg}", flush=True)


def kill_ide():
    import psutil
    killed = 0
    for proc in psutil.process_iter(["pid", "name"]):
        try:
            pname = (proc.info["name"] or "").lower()
            if "wechatdevtools" in pname:
                proc.kill()
                killed += 1
        except Exception:
            pass
    if killed:
        log(f"Killed {killed} IDE processes")
        time.sleep(5)


def clear_logs():
    if os.path.isdir(LOG_DIR):
        cnt = 0
        for f in os.listdir(LOG_DIR):
            if f.endswith(".log"):
                try:
                    os.remove(os.path.join(LOG_DIR, f))
                    cnt += 1
                except Exception:
                    pass
        log(f"Cleared {cnt} log files")


def start_cdn():
    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=CDN_ROOT, **kwargs)
        def log_message(self, *args): pass
    socketserver.TCPServer.allow_reuse_address = True
    httpd = socketserver.TCPServer(("0.0.0.0", 8000), Handler)
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    log("CDN server started on :8000")
    return httpd


def open_ide_with_http_port():
    """Open IDE with --port to enable HTTP API"""
    log(f"Opening IDE with --port {HTTP_PORT}...")
    proc = subprocess.Popen(
        [CLI, "open", "--project", PROJECT, "--port", str(HTTP_PORT)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    log(f"open dispatched, PID={proc.pid}")
    log("Waiting 45s for IDE to load + open HTTP port...")
    time.sleep(45)

    # Check if HTTP port is listening
    try:
        with urllib.request.urlopen(f"http://127.0.0.1:{HTTP_PORT}/open", timeout=3) as resp:
            log(f"HTTP API on port {HTTP_PORT} is UP (status={resp.status})")
            return True
    except Exception as e:
        log(f"HTTP API on port {HTTP_PORT} not available: {e}")
        return False


def probe_and_compile():
    """Probe HTTP API for compile-related endpoints"""
    log("Probing HTTP API endpoints...")

    # Try various endpoints that might trigger compile
    endpoints_to_try = [
        # Try POST with project path
        ("POST", "/v2/open", json.dumps({"projectPath": PROJECT}).encode()),
        ("POST", "/open", json.dumps({"projectPath": PROJECT}).encode()),
        # Try auto-preview (might trigger compile)
        ("GET", f"/auto-preview?projectPath={PROJECT}", None),
        ("GET", f"/v2/auto-preview?projectPath={PROJECT}", None),
        # Try preview (might trigger compile internally)
        ("GET", f"/preview?projectPath={PROJECT}", None),
        ("GET", f"/v2/preview?projectPath={PROJECT}", None),
        # Try compile directly
        ("GET", "/compile", None),
        ("GET", "/v2/compile", None),
        ("POST", "/compile", json.dumps({"projectPath": PROJECT}).encode()),
        ("POST", "/v2/compile", json.dumps({"projectPath": PROJECT}).encode()),
        # Try restart
        ("GET", "/restart", None),
        ("POST", "/restart", None),
    ]

    for method, path, body in endpoints_to_try:
        url = f"http://127.0.0.1:{HTTP_PORT}{path.split('?')[0]}"
        try:
            req = urllib.request.Request(url, method=method, data=body)
            if body:
                req.add_header("Content-Type", "application/json")
            with urllib.request.urlopen(req, timeout=10) as resp:
                resp_body = resp.read(500).decode("utf-8", errors="ignore")
                log(f"  {method} {path} -> {resp.status} {resp_body[:200]}")
        except urllib.error.HTTPError as e:
            resp_body = e.read(500).decode("utf-8", errors="ignore") if e.fp else ""
            log(f"  {method} {path} -> {e.code} {resp_body[:200]}")
        except Exception as e:
            log(f"  {method} {path} -> ERR {type(e).__name__}")


def monitor_logs(timeout=180):
    log(f"Monitoring logs for {timeout}s...")
    deadline = time.time() + timeout
    last_check = 0
    while time.time() < deadline:
        if os.path.isdir(LOG_DIR):
            for f in sorted(os.listdir(LOG_DIR), reverse=True):
                if not f.endswith(".log"):
                    continue
                full = os.path.join(LOG_DIR, f)
                try:
                    mtime = os.path.getmtime(full)
                    if mtime < last_check:
                        continue
                    with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                        content = fh.read()
                    # Check for game keywords
                    for kw in ["[WeChat]", "[C#]", "Main._Ready", "Godot Engine",
                                "WASM instantiation", "Game started", "Game failed",
                                "canvas=", "Engine start error", "CANNOT HANDLE",
                                "CompileError", "TypeError", "ReferenceError"]:
                        if kw in content:
                            log(f"Found '{kw}' in {f}")
                            # Print relevant lines
                            for line in content.splitlines():
                                if any(k in line for k in ["[WeChat]", "[C#]", "Godot", "WASM",
                                                            "canvas=", "Game started", "Game failed",
                                                            "Engine", "ERROR", "error", "Compile",
                                                            "TypeError", "ReferenceError"]):
                                    print(f"  {line}", flush=True)
                            return True
                except Exception:
                    pass
        time.sleep(3)
    return False


def main():
    log("=== Run with HTTP Port ===")
    kill_ide()
    clear_logs()
    start_cdn()
    time.sleep(2)

    if not open_ide_with_http_port():
        log("HTTP API not available, aborting")
        return 1

    probe_and_compile()

    found = monitor_logs(180)
    if found:
        log("PASS: Game activity detected!")
        return 0
    else:
        log("FAIL: No game activity in logs")
        return 1


if __name__ == "__main__":
    sys.exit(main())
