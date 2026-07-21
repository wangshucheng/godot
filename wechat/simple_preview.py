"""simple_preview.py - 最简方案：直接用 cli.bat preview 触发编译"""
import subprocess
import time
import os
import sys
import threading
import http.server
import socketserver

CLI = r"D:\software\Tencent\微信web开发者工具\cli.bat"
PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
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
        time.sleep(3)


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


def run_preview():
    """Run cli.bat preview --project X --qr-output console"""
    log("Running cli.bat preview...")
    try:
        result = subprocess.run(
            [CLI, "preview", "--project", PROJECT, "--qr-output", "console"],
            capture_output=True, text=True, timeout=180, encoding="utf-8", errors="ignore"
        )
        log(f"preview exit code: {result.returncode}")
        out = (result.stdout or "") + (result.stderr or "")
        # Print last 50 lines
        lines = out.splitlines()
        log(f"Output ({len(lines)} lines):")
        for line in lines[-50:]:
            print(f"  {line}", flush=True)
        return result.returncode == 0, out
    except subprocess.TimeoutExpired:
        log("preview timed out after 180s")
        return False, ""
    except Exception as e:
        log(f"preview exception: {e}")
        return False, ""


def monitor_logs(timeout=120):
    log(f"Monitoring logs for {timeout}s...")
    deadline = time.time() + timeout
    while time.time() < deadline:
        if os.path.isdir(LOG_DIR):
            for f in sorted(os.listdir(LOG_DIR), reverse=True):
                if not f.endswith(".log"):
                    continue
                full = os.path.join(LOG_DIR, f)
                try:
                    with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                        content = fh.read()
                    # Check for game keywords
                    for kw in ["[WeChat]", "[C#]", "Main._Ready", "Godot Engine",
                                "WASM instantiation", "Game started", "Game failed",
                                "canvas=", "Engine start error"]:
                        if kw in content:
                            # Print last 10 lines containing these keywords
                            log(f"Found keyword '{kw}' in {f}")
                            for line in content.splitlines():
                                if any(k in line for k in ["[WeChat]", "[C#]", "Godot", "WASM",
                                                            "canvas=", "Game started", "Game failed",
                                                            "Engine", "ERROR", "error"]):
                                    print(f"  {line}", flush=True)
                            return True
                except Exception:
                    pass
        time.sleep(3)
    return False


def main():
    log("=== Simple Preview Test ===")
    kill_ide()
    clear_logs()
    cdn = start_cdn()
    time.sleep(2)

    # First open IDE with project
    log("Opening IDE with project...")
    proc = subprocess.Popen(
        [CLI, "open", "--project", PROJECT],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    log(f"open dispatched, PID={proc.pid}")
    log("Waiting 60s for IDE to load project...")
    time.sleep(60)

    # Check if project loaded
    import win32gui
    windows = []
    def cb(hwnd, _):
        if win32gui.IsWindowVisible(hwnd):
            title = win32gui.GetWindowText(hwnd) or ""
            if "微信开发者工具" in title:
                windows.append(title)
    win32gui.EnumWindows(cb, None)
    log(f"IDE windows: {windows}")

    # Try cli.bat preview to trigger compile
    ok, _ = run_preview()

    # Monitor logs
    found = monitor_logs(120)
    if found:
        log("PASS: Game activity detected in logs!")
        return 0
    else:
        log("FAIL: No game activity in logs")
        return 1


if __name__ == "__main__":
    sys.exit(main())
