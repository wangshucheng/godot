"""
full_e2e.py - 完整端到端自动化测试

完整流程:
1. 杀掉 IDE 进程 + 旧 HTTP 服务器
2. 清空 WeappLog
3. 启动 HTTP CDN 服务器 (端口 8000)
4. cli.bat open --project 打开 IDE 加载项目
5. 等待 IDE 完全加载 (45s)
6. 监控日志 60s 看是否自动编译运行
7. 如果没编译，用 SendInput 发送 Ctrl+B 触发编译
8. 继续监控 120s，看 canvas= 尺寸是否 >100x100
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time
import threading
import ctypes
import ctypes.wintypes as w
from pathlib import Path

import psutil

try:
    import win32gui
    import win32con
    import win32api
    HAS_WIN32 = True
except ImportError:
    HAS_WIN32 = False


DEFAULT_PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
DEFAULT_CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
DEFAULT_CLI = r"D:\software\Tencent\微信web开发者工具\cli.bat"
DEVTOOLS_PROCESS_NAMES = ["wechatdevtools", "wechatdevtools-helper", "WeAppExe", "WeappPlayer", "WeAppPlayer"]


# ANSI colors
def log(msg, level="info"):
    colors = {"info": "\033[33m", "ok": "\033[32m", "err": "\033[31m",
              "step": "\033[36m", "log": "\033[90m", "reset": "\033[0m"}
    c = colors.get(level, "")
    r = colors["reset"]
    ts = time.strftime("%H:%M:%S")
    if level == "step":
        print(f"\n{c}[{ts}] === {msg} ==={r}", flush=True)
    elif level == "log":
        print(f"  {c}LOG: {msg}{r}", flush=True)
    else:
        prefix = {"info": "[i]", "ok": "[OK]", "err": "[ERR]"}[level]
        print(f"{c}[{ts}] {prefix} {msg}{r}", flush=True)


def kill_devtools():
    log("Killing existing DevTools processes", "step")
    killed = 0
    for proc in psutil.process_iter(["pid", "name"]):
        try:
            pname = proc.info["name"] or ""
            if pname.lower().endswith(".exe"):
                pname_base = pname[:-4].lower()
                if pname_base in [n.lower() for n in DEVTOOLS_PROCESS_NAMES]:
                    proc.kill()
                    killed += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            continue
    if killed:
        log(f"Killed {killed} processes", "ok")
        time.sleep(3)
    else:
        log("No DevTools process running", "info")


def kill_port(port):
    try:
        for conn in psutil.net_connections(kind="inet"):
            if conn.laddr.port == port and conn.status == "LISTEN":
                try:
                    psutil.Process(conn.pid).kill()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
        time.sleep(1)
    except Exception:
        pass


def find_log_dirs():
    local_appdata = os.environ.get("LOCALAPPDATA", r"C:\Users\Administrator\AppData\Local")
    user_data_root = os.path.join(local_appdata, "微信开发者工具", "User Data")
    patterns = [
        os.path.join(user_data_root, "*", "WeappLog", "logs"),
        os.path.join(user_data_root, "*", "Default", "WeappLog", "logs"),
        os.path.join(user_data_root, "Default", "WeappLog", "logs"),
    ]
    found = []
    seen = set()
    for p in patterns:
        for m in glob.glob(p):
            if m not in seen:
                seen.add(m)
                found.append(m)
    return found


def clear_weapp_logs():
    log("Clearing old WeappLog files", "step")
    log_dirs = find_log_dirs()
    total = 0
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if f.endswith(".log"):
                    try:
                        os.remove(os.path.join(d, f))
                        total += 1
                    except Exception:
                        pass
        except Exception:
            continue
    log(f"Cleared {total} old log file(s)", "ok")
    return time.time()  # Return clear time for filtering


def start_http_server(cdn_root, port):
    log(f"Starting HTTP CDN server on port {port}", "step")
    if not os.path.isdir(cdn_root):
        log(f"CDN root not found: {cdn_root}", "err")
        return None
    import http.server
    import socketserver
    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=cdn_root, **kwargs)
        def log_message(self, format, *args):
            pass
    socketserver.TCPServer.allow_reuse_address = True
    httpd = socketserver.TCPServer(("0.0.0.0", port), Handler)
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    time.sleep(1)
    try:
        import urllib.request
        with urllib.request.urlopen(f"http://localhost:{port}/", timeout=5) as resp:
            if resp.status == 200:
                log(f"HTTP server OK on port {port}", "ok")
                return httpd
    except Exception as e:
        log(f"HTTP server not responding: {e}", "err")
        return None


def open_ide_with_project(cli_path, project_path):
    log(f"Opening IDE with project", "step")
    if not os.path.isfile(cli_path):
        log(f"CLI not found: {cli_path}", "err")
        return False
    if not os.path.isdir(project_path):
        log(f"Project path not found: {project_path}", "err")
        return False
    try:
        # Launch and wait briefly
        proc = subprocess.Popen(
            [cli_path, "open", "--project", project_path],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        log(f"IDE launch dispatched (PID={proc.pid})", "ok")
        return True
    except Exception as e:
        log(f"Failed to launch IDE: {e}", "err")
        return False


def find_ide_window():
    if not HAS_WIN32:
        return None
    results = []
    def cb(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        title = win32gui.GetWindowText(hwnd) or ""
        if "微信开发者工具" in title or "2048" in title:
            results.append((hwnd, title, win32gui.GetWindowRect(hwnd)))
    win32gui.EnumWindows(cb, None)
    return results


def focus_ide_window():
    if not HAS_WIN32:
        return None
    windows = find_ide_window()
    if not windows:
        return None
    # Prefer one with 2048 in title
    hwnd, title, rect = windows[0]
    for h, t, r in windows:
        if "2048" in t or "minigame" in t.lower():
            hwnd, title, rect = h, t, r
            break

    if win32gui.IsIconic(hwnd):
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(0.3)
    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040
    win32gui.SetWindowPos(hwnd, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW)
    time.sleep(0.3)
    ASFW_ANY = -1
    try:
        win32gui.AllowSetForegroundWindow(ASFW_ANY)
    except Exception:
        pass
    win32api.keybd_event(0x12, 0, 0, 0)
    win32api.keybd_event(0x12, 0, 0x0002, 0)
    win32gui.SetForegroundWindow(hwnd)
    time.sleep(1.0)
    return (hwnd, title, rect) if win32gui.GetForegroundWindow() == hwnd else None


# SendInput setup
SendInput = ctypes.windll.user32.SendInput
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002

class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", w.WORD),
                ("wScan", w.WORD),
                ("dwFlags", w.DWORD),
                ("time", w.DWORD),
                ("dwExtraInfo", ctypes.POINTER(w.ULONG))]

class INPUT_UNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT)]

class INPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", w.DWORD),
                ("u", INPUT_UNION)]


def send_key_combo(vk_codes, hold_time=0.15):
    inputs = []
    for vk in vk_codes:
        i = INPUT()
        i.type = INPUT_KEYBOARD
        i.ki.wVk = vk
        i.ki.wScan = 0
        i.ki.dwFlags = 0
        i.ki.time = 0
        i.ki.dwExtraInfo = ctypes.pointer(w.ULONG(0))
        inputs.append(i)
    n = len(inputs)
    arr = (INPUT * n)(*inputs)
    SendInput(n, ctypes.pointer(arr[0]), ctypes.sizeof(INPUT))
    time.sleep(hold_time)
    rels = []
    for vk in reversed(vk_codes):
        i = INPUT()
        i.type = INPUT_KEYBOARD
        i.ki.wVk = vk
        i.ki.wScan = 0
        i.ki.dwFlags = KEYEVENTF_KEYUP
        i.ki.time = 0
        i.ki.dwExtraInfo = ctypes.pointer(w.ULONG(0))
        rels.append(i)
    n = len(rels)
    arr = (INPUT * n)(*rels)
    SendInput(n, ctypes.pointer(arr[0]), ctypes.sizeof(INPUT))


def scan_logs_for_keywords(start_after=0):
    """扫描 WeappLog，返回 (success_keywords_found, fail_keywords_found, canvas_size, last_30_lines)."""
    success_keywords = ["[WeChat] Game started", "[C#] Main._Ready", "Godot Engine v4", "WASM instantiation succeeded", "[WeChat] Starting Godot"]
    fail_keywords = ["Game failed to start", "WASM instantiation failed", "CANNOT HANDLE COOKIE", "[WeChat] Engine start error"]
    canvas_re = re.compile(r'canvas=(\d+)x(\d+)')

    found_success = []
    found_fail = []
    canvas_size = ""
    all_recent_lines = []

    log_dirs = find_log_dirs()
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if not f.endswith(".log"):
                    continue
                full = os.path.join(d, f)
                if os.path.getmtime(full) < start_after:
                    continue
                try:
                    with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                        content = fh.read()
                except Exception:
                    continue
                for kw in success_keywords:
                    if kw in content and kw not in found_success:
                        found_success.append(kw)
                for kw in fail_keywords:
                    if kw in content and kw not in found_fail:
                        found_fail.append(kw)
                m = canvas_re.search(content)
                if m and not canvas_size:
                    canvas_size = f"{m.group(1)}x{m.group(2)}"
                # Last 30 lines
                lines = content.splitlines()[-30:]
                for line in lines:
                    if any(kw in line for kw in success_keywords + fail_keywords) or "canvas=" in line:
                        all_recent_lines.append(line)
        except Exception:
            continue
    return found_success, found_fail, canvas_size, all_recent_lines


def monitor_logs(timeout_sec=120, start_after=0, check_interval=2.0):
    """监控日志，直到成功或超时。"""
    log(f"Monitoring logs for {timeout_sec}s (start_after={start_after:.0f})", "step")
    deadline = time.time() + timeout_sec
    last_success_count = 0
    last_canvas = ""
    last_print_time = 0

    while time.time() < deadline:
        time.sleep(check_interval)
        found_success, found_fail, canvas_size, _ = scan_logs_for_keywords(start_after)

        # Print new findings
        if len(found_success) > last_success_count or canvas_size != last_canvas:
            for kw in found_success[last_success_count:]:
                log(f"Found success marker: {kw}", "ok")
            last_success_count = len(found_success)
            if canvas_size != last_canvas:
                log(f"Canvas size: {canvas_size}", "info")
                last_canvas = canvas_size

        if found_fail:
            log(f"FAIL markers: {found_fail}", "err")
            return "FAIL", found_fail[0], canvas_size

        # Success condition: Game started AND canvas > 100x100
        if any("[WeChat] Game started" in s or "[C#] Main._Ready" in s for s in found_success):
            if canvas_size:
                m = re.match(r"(\d+)x(\d+)", canvas_size)
                if m and int(m.group(1)) > 100 and int(m.group(2)) > 100:
                    log(f"PASS: Game started with valid canvas {canvas_size}", "ok")
                    return "PASS", "", canvas_size
                else:
                    log(f"Game started but canvas invalid: {canvas_size}", "err")
                    return "FAIL", f"canvas_invalid_{canvas_size}", canvas_size

    return "TIMEOUT", "", last_canvas


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", default=DEFAULT_PROJECT)
    parser.add_argument("--cdn-root", default=DEFAULT_CDN_ROOT)
    parser.add_argument("--cli", default=DEFAULT_CLI)
    parser.add_argument("--no-restart-ide", action="store_true", help="Skip killing/relaunching IDE")
    parser.add_argument("--monitor-timeout", type=int, default=180)
    args = parser.parse_args()

    log("Full E2E Automated Test", "step")

    # Step 1: Kill IDE
    if not args.no_restart_ide:
        kill_devtools()
        kill_port(8000)
        kill_port(9999)

    # Step 2: Clear logs
    clear_time = clear_weapp_logs()

    # Step 3: Start HTTP CDN server
    httpd = start_http_server(args.cdn_root, 8000)
    if not httpd:
        log("HTTP server failed, aborting", "err")
        return 1

    # Step 4: Open IDE
    if not args.no_restart_ide:
        if not open_ide_with_project(args.cli, args.project):
            log("Failed to open IDE", "err")
            return 1
        log("Waiting 45s for IDE to fully load...", "step")
        time.sleep(45)
    else:
        log("IDE restart skipped (already running)", "info")

    # Step 5: Check if IDE auto-compiled (monitor 30s first)
    log("Checking if IDE auto-compiled (30s)...", "step")
    result, reason, canvas = monitor_logs(timeout_sec=30, start_after=clear_time)
    if result == "PASS":
        log(f"TEST PASSED on first try! canvas={canvas}", "ok")
        return 0

    # Step 6: Try SendInput Ctrl+B
    log("Auto-compile not detected. Trying Ctrl+B via SendInput...", "step")
    ide = focus_ide_window()
    if not ide:
        log("Could not focus IDE window", "err")
    else:
        hwnd, title, rect = ide
        log(f"IDE focused: {title}", "ok")
        time.sleep(2)

        # Ctrl+B
        VK_CTRL = 0x11
        VK_B = 0x42
        VK_R = 0x52
        VK_F5 = 0x74

        for vks, name in [([VK_CTRL, VK_B], "Ctrl+B"),
                          ([VK_CTRL, VK_R], "Ctrl+R"),
                          ([VK_F5], "F5")]:
            log(f"Sending {name}...", "info")
            try:
                send_key_combo(vks)
                log(f"Sent {name}", "ok")
            except Exception as e:
                log(f"SendInput failed: {e}", "err")
            # Monitor 20s for effect
            result, reason, canvas = monitor_logs(timeout_sec=20, start_after=clear_time)
            if result == "PASS":
                log(f"TEST PASSED after {name}! canvas={canvas}", "ok")
                return 0
            if result == "FAIL":
                log(f"FAIL after {name}: {reason}", "err")
                return 1

    # Step 7: Final monitoring
    remaining = max(30, args.monitor_timeout - 90)  # We've used ~90s so far
    log(f"Final monitoring for {remaining}s...", "step")
    result, reason, canvas = monitor_logs(timeout_sec=remaining, start_after=clear_time)

    if result == "PASS":
        log(f"TEST PASSED! canvas={canvas}", "ok")
        return 0
    elif result == "FAIL":
        log(f"TEST FAILED: {reason}", "err")
        return 1
    else:
        # TIMEOUT - dump last logs
        log("TIMEOUT - dumping recent log lines...", "err")
        _, _, _, recent = scan_logs_for_keywords(start_after=clear_time)
        for line in recent[-30:]:
            log(line, "log")
        return 2


if __name__ == "__main__":
    sys.exit(main())
