#!/usr/bin/env python3
"""
test_e2e.py - 微信小游戏端到端自动化测试（全自动模式）

流程:
  1. 杀掉微信开发者工具 + 旧 HTTP 服务器
  2. 清理 IDE workspace 缓存 + 旧 WeappLog 日志
  3. 启动本地 HTTP CDN 服务器（端口 8000）
  4. 用 CLI 打开 IDE 加载项目（自动编译）
  5. 若 60s 内无游戏运行日志，调用 cli.bat preview 触发编译
  6. 监控新产生的 WeappLog，检测关键标志判断成功/失败
  7. 失败则循环重试（最多 3 轮）

成功条件:
  - "[WeChat] Game started successfully!" 或 "[C#] Main._Ready() called!"
  - 且 canvas 尺寸 > 100x100

失败条件:
  - canvas 尺寸 <= 10x10（3x3 bug 未修复）
  - "Game failed to start" / "WASM instantiation failed"
  - 超时
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

import psutil

try:
    import win32gui
    import win32con
    HAS_WIN32 = True
except ImportError:
    HAS_WIN32 = False


DEFAULT_PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
DEFAULT_CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
DEFAULT_CLI = r"D:\software\Tencent\微信web开发者工具\cli.bat"
DEVTOOLS_PROCESS_NAMES = ["wechatdevtools", "wechatdevtools-helper", "WeAppExe", "WeappPlayer", "WeAppPlayer"]


def log(msg, level="info"):
    colors = {"info": "\033[33m", "ok": "\033[32m", "err": "\033[31m",
              "step": "\033[36m", "log": "\033[90m", "reset": "\033[0m"}
    c = colors.get(level, "")
    r = colors["reset"]
    ts = time.strftime("%H:%M:%S")
    if level == "step":
        print(f"\n{c}[{ts}] === {msg} ==={r}")
    elif level == "log":
        print(f"  {c}LOG: {msg}{r}")
    else:
        prefix = {"info": "[i]", "ok": "[OK]", "err": "[ERR]"}[level]
        print(f"{c}[{ts}] {prefix} {msg}{r}")


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
    if killed == 0:
        log("No DevTools process running", "info")
    else:
        log(f"Killed {killed} processes", "ok")
        time.sleep(3)
    return killed


def kill_port(port):
    try:
        for conn in psutil.net_connections(kind="inet"):
            if conn.laddr.port == port and conn.status == "LISTEN":
                try:
                    p = psutil.Process(conn.pid)
                    p.kill()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
        time.sleep(2)
    except Exception:
        pass


def clear_workspace_cache():
    log("Clearing IDE workspace cache", "step")
    workspace_dir = os.path.join(
        os.environ.get("LOCALAPPDATA", ""),
        "微信开发者工具", "User Data", "Default",
        "Editor", "1.78", "user-data", "User", "workspaceStorage"
    )
    if os.path.isdir(workspace_dir):
        try:
            shutil.rmtree(workspace_dir, ignore_errors=True)
            log("Cleared workspaceStorage", "ok")
        except Exception as e:
            log(f"workspaceStorage clear failed: {e}", "info")
    else:
        log("No workspaceStorage dir", "info")


def clear_weapp_logs():
    """清空所有 WeappLog 目录下的 .log 文件，确保后续看到的全是新日志。"""
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


def start_http_server(cdn_root, port):
    log(f"Starting HTTP CDN server on port {port}", "step")
    if not os.path.isdir(cdn_root):
        log(f"CDN root not found: {cdn_root}", "err")
        return None

    import http.server
    import socketserver
    import threading

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
                log(f"HTTP server responding on port {port}", "ok")
                return httpd
    except Exception as e:
        log(f"HTTP server not responding: {e}", "err")
        return None


def open_ide_with_project(cli_path, project_path):
    log(f"Opening WeChat DevTools with project", "step")
    if not os.path.isfile(cli_path):
        log(f"CLI not found: {cli_path}", "err")
        return False
    if not os.path.isdir(project_path):
        log(f"Project path not found: {project_path}", "err")
        return False

    try:
        proc = subprocess.Popen(
            [cli_path, "open", "--project", project_path],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        log(f"IDE launch dispatched (launcher PID={proc.pid})", "ok")
        return True
    except Exception as e:
        log(f"Failed to launch IDE: {e}", "err")
        return False


def trigger_compile_via_cli_preview(cli_path, project_path, timeout=120):
    """通过 cli.bat preview 触发编译。

    cli.bat preview 会启动 IDE server (端口 15976)，编译项目并生成预览。
    虽然主要用途是生成预览二维码，但它确实会触发完整编译流程，
    编译过程中如果 IDE 已打开，会在 IDE 内重新加载并运行游戏。

    返回 (success: bool, output: str)
    """
    log("Triggering compile via cli.bat preview", "step")
    if not os.path.isfile(cli_path):
        log(f"CLI not found: {cli_path}", "err")
        return False, ""

    try:
        # 用 subprocess.run 等待完成，捕获输出
        result = subprocess.run(
            [cli_path, "preview", "--project", project_path, "--qr-output", "console"],
            capture_output=True, text=True, timeout=timeout, encoding="utf-8",
            errors="ignore"
        )
        output = (result.stdout or "") + (result.stderr or "")
        # 截取关键部分
        if "✔ preview" in output or "✔ Preview" in output:
            log("cli.bat preview succeeded (✔ preview)", "ok")
            # 提取大小信息
            for line in output.splitlines():
                if "TOTAL" in line or "main" in line.lower() or "wasm_pkg" in line:
                    log(f"  {line.strip()}", "info")
            return True, output
        elif "preview" in output.lower() and ("error" in output.lower() or "fail" in output.lower()):
            log(f"cli.bat preview failed", "err")
            # 打印最后 30 行
            for line in output.splitlines()[-30:]:
                log(f"  {line.strip()}", "info")
            return False, output
        else:
            log(f"cli.bat preview completed (exit={result.returncode})", "info")
            for line in output.splitlines()[-15:]:
                log(f"  {line.strip()}", "info")
            return result.returncode == 0, output
    except subprocess.TimeoutExpired:
        log(f"cli.bat preview timed out after {timeout}s", "err")
        return False, ""
    except Exception as e:
        log(f"cli.bat preview exception: {e}", "err")
        return False, ""


def find_ide_window():
    """找到 IDE 主窗口并拉到前台。"""
    if not HAS_WIN32:
        return None

    try:
        import win32api
    except ImportError:
        pass

    results = []
    def callback(hwnd, _):
        title = win32gui.GetWindowText(hwnd) or ""
        if "微信开发者工具" not in title:
            return
        rect = win32gui.GetWindowRect(hwnd)
        results.append((hwnd, title, rect))
    win32gui.EnumWindows(callback, None)
    if not results:
        return None

    hwnd, title, rect = results[0]
    for h, t, r in results:
        if "2048" in t or "minigame" in t.lower():
            hwnd, title, rect = h, t, r
            break

    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040

    try:
        left, top, right, bottom = rect
        if left < -1000 or top < -1000:
            log(f"Window minimized at {rect}, restoring...", "info")
            win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
            time.sleep(0.5)
            win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
            time.sleep(0.5)
            win32gui.SetWindowPos(hwnd, 0, 100, 100, 0, 0,
                                  SWP_NOSIZE | SWP_SHOWWINDOW)
            time.sleep(0.5)
        else:
            win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
            time.sleep(0.2)

        ASFW_ANY = -1
        try:
            win32gui.AllowSetForegroundWindow(ASFW_ANY)
        except Exception:
            pass
        try:
            win32api.keybd_event(0x12, 0, 0, 0)
            win32api.keybd_event(0x12, 0, 0x0002, 0)
        except Exception:
            pass
        win32gui.SetForegroundWindow(hwnd)
        time.sleep(1)
    except Exception as e:
        log(f"Window restore failed: {e}", "info")

    try:
        rect = win32gui.GetWindowRect(hwnd)
    except Exception:
        pass
    return (hwnd, title, rect)


def trigger_compile_keystroke():
    """通过 Ctrl+R / Ctrl+B / F5 快捷键触发编译（备用方案）。"""
    if not HAS_WIN32:
        return False
    try:
        import win32api
        import win32con
    except ImportError:
        return False

    ide = find_ide_window()
    if not ide:
        return False
    hwnd, title, rect = ide
    log(f"IDE in foreground: {title} rect={rect}", "ok")
    time.sleep(2)

    def send_combo(vk_codes):
        for vk in vk_codes:
            win32api.keybd_event(vk, 0, 0, 0)
            time.sleep(0.05)
        time.sleep(0.15)
        for vk in reversed(vk_codes):
            win32api.keybd_event(vk, 0, win32con.KEYEVENTF_KEYUP, 0)
            time.sleep(0.05)

    VK_R, VK_B = 0x52, 0x42
    for combo, name in [([win32con.VK_CONTROL, VK_R], "Ctrl+R"),
                         ([win32con.VK_CONTROL, VK_B], "Ctrl+B"),
                         ([win32con.VK_F5], "F5")]:
        try:
            send_combo(combo)
            log(f"Sent {name}", "ok")
        except Exception as e:
            log(f"{name} failed: {e}", "info")
        time.sleep(8)
        if has_game_runtime_log_recent(seconds=20):
            log(f"Game logs detected after {name}", "ok")
            return True
    return False


def has_game_runtime_log_recent(seconds=60):
    """检查最近 N 秒内是否有游戏运行日志。"""
    log_dirs = find_log_dirs()
    game_keywords = ["[WeChat]", "[C#]", "[Mono]", "canvas=", "Game started",
                     "Main._Ready", "Godot Engine", "WASM instantiation",
                     "_instantiateWasm"]
    cutoff = time.time() - seconds
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if not f.endswith(".log"):
                    continue
                full = os.path.join(d, f)
                if os.path.getmtime(full) < cutoff:
                    continue
                try:
                    with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                        content = fh.read()
                    for kw in game_keywords:
                        if kw in content:
                            return True
                except Exception:
                    continue
        except Exception:
            continue
    return False


def find_log_dirs():
    """查找所有可能的 WeappLog 目录。"""
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


def monitor_logs(timeout_sec=300, start_after=0):
    """监控 WeappLog 目录（多路径）。

    start_after: 只检查 mtime > start_after 的日志文件（用于过滤旧日志）
    """
    log(f"Monitoring WeappLog for {timeout_sec}s (start_after={start_after})", "step")

    deadline = time.time() + timeout_sec
    latest_log = None
    last_pos = 0
    last_canvas_size = ""
    success_evidences = []
    result = "TIMEOUT"
    fail_reason = ""

    log_dirs = find_log_dirs()
    if not log_dirs:
        log("No WeappLog dir found", "err")
        return "FAIL", "No WeappLog dir", "", []

    log(f"Found {len(log_dirs)} log dir(s)", "ok")
    for d in log_dirs:
        log(f"  {d}", "info")

    game_keyword_re = re.compile(
        r'\[WeChat\]|\[Mono\]|\[C#\]|Godot Engine|WASM|Game started|Game failed|canvas=|_Ready|_instantiateWasm|WXWebAssembly'
    )
    error_re = re.compile(r'ERROR|Failed|TypeError|ReferenceError|CompileError|RuntimeError|FATAL', re.IGNORECASE)

    while time.time() < deadline:
        # 找最新日志文件（mtime > start_after）
        all_logs = []
        for d in log_dirs:
            try:
                for f in os.listdir(d):
                    if not f.endswith(".log"):
                        continue
                    full = os.path.join(d, f)
                    mt = os.path.getmtime(full)
                    if start_after > 0 and mt < start_after:
                        continue
                    all_logs.append((mt, full))
            except Exception:
                continue

        if not all_logs:
            time.sleep(3)
            continue

        all_logs.sort(reverse=True)
        current_log = all_logs[0][1]

        if current_log != latest_log:
            latest_log = current_log
            last_pos = 0
            log(f"Following: {os.path.basename(latest_log)}", "info")

        try:
            with open(latest_log, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()
            if not content:
                time.sleep(3)
                continue

            start = min(last_pos, len(content))
            new_content = content[start:]
            last_pos = len(content)

            if not new_content:
                time.sleep(3)
                continue

            # 打印游戏相关 + 错误相关行
            for line in new_content.splitlines():
                line = line.strip()
                if not line:
                    continue
                if game_keyword_re.search(line) or error_re.search(line):
                    # 截断过长行
                    display = line if len(line) < 200 else line[:200] + "..."
                    log(display, "log")

            if "[WeChat] Game started successfully" in new_content:
                if "Game started successfully" not in success_evidences:
                    success_evidences.append("Game started successfully")

            if "[C#] Main._Ready() called" in new_content:
                if "C# Main._Ready() called" not in success_evidences:
                    success_evidences.append("C# Main._Ready() called")

            for m in re.finditer(r'canvas=(\d+)x(\d+)', new_content):
                w, h = int(m.group(1)), int(m.group(2))
                last_canvas_size = f"{w}x{h}"
                if w > 100 and h > 100:
                    if f"Canvas size OK: {w}x{h}" not in success_evidences:
                        success_evidences.append(f"Canvas size OK: {w}x{h}")
                elif w <= 10 and h <= 10:
                    fail_reason = f"Canvas shrunk to {w}x{h} (3x3 bug not fixed)"
                    result = "FAIL"
                    break

            if re.search(r'Game failed to start|WASM instantiation failed|FATAL|RuntimeError', new_content):
                fail_reason = "Engine startup failed"
                result = "FAIL"
                break

            has_startup = any("Game started successfully" in e or "C# Main._Ready" in e for e in success_evidences)
            has_canvas_ok = any("Canvas size OK" in e for e in success_evidences)
            if has_startup and has_canvas_ok:
                result = "PASS"
                break

        except Exception:
            pass

        time.sleep(3)

    return result, fail_reason, last_canvas_size, success_evidences


def print_log_tail(log_path, max_lines=80):
    if not log_path or not os.path.isfile(log_path):
        return
    log("Latest log tail (key lines)", "step")
    try:
        with open(log_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
        key_patterns = re.compile(
            r'\[WeChat\]|\[Mono\]|\[C#\]|Godot Engine|WASM|Game started|Game failed|canvas=|ERROR|Failed|TypeError|ReferenceError|CompileError|RuntimeError|instantiateWasm|WXWebAssembly|_Ready'
        )
        lines = [l for l in content.splitlines() if key_patterns.search(l)]
        for line in lines[-max_lines:]:
            print(f"  {line}")
    except Exception:
        pass


def find_latest_log():
    """返回最新的 .log 文件路径（跨所有 log_dir）。"""
    log_dirs = find_log_dirs()
    if not log_dirs:
        return None
    all_logs = []
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if f.endswith(".log"):
                    full = os.path.join(d, f)
                    all_logs.append((os.path.getmtime(full), full))
        except Exception:
            continue
    if not all_logs:
        return None
    all_logs.sort(reverse=True)
    return all_logs[0][1]


def run_one_cycle(args, cycle_num, max_cycles):
    """运行一轮测试。返回 (result, fail_reason, last_canvas, evidences)。"""
    log(f"Test cycle {cycle_num}/{max_cycles}", "step")

    # Step 1: 杀进程
    kill_devtools()
    kill_port(args.cdn_port)

    # Step 2: 清缓存 + 清旧日志
    clear_workspace_cache()
    clear_weapp_logs()
    cycle_start_time = time.time()

    # Step 3: 起 HTTP 服务器
    httpd = start_http_server(args.cdn_root, args.cdn_port)
    if not httpd:
        return "FAIL", "Cannot start HTTP server", "", []

    # Step 4: 打开 IDE（会自动编译）
    if not open_ide_with_project(args.cli, args.project):
        return "FAIL", "Cannot open IDE", "", []

    log("Waiting 25s for IDE to launch and auto-compile...", "info")
    time.sleep(25)

    # Step 5: 检查是否已自动编译（看是否有游戏运行日志）
    if has_game_runtime_log_recent(seconds=60):
        log("Auto-compile detected game logs after IDE open", "ok")
    else:
        log("No game logs after IDE open, trying keystroke trigger", "info")
        trigger_compile_keystroke()
        time.sleep(15)

        if has_game_runtime_log_recent(seconds=30):
            log("Game logs detected after keystroke", "ok")
        else:
            # Step 6: 用 cli.bat preview 触发编译
            log("No game logs after keystroke, trying cli.bat preview", "step")
            ok, _ = trigger_compile_via_cli_preview(args.cli, args.project, timeout=120)
            time.sleep(20)

            if has_game_runtime_log_recent(seconds=60):
                log("Game logs detected after cli.bat preview", "ok")
            else:
                log("Still no game logs, will monitor longer", "info")

    # Step 7: 监控日志
    result, fail_reason, last_canvas, evidences = monitor_logs(
        timeout_sec=args.timeout, start_after=cycle_start_time - 30
    )

    # Step 8: 输出结果
    log(f"Cycle {cycle_num} result: {result}", "step")
    log(f"Latest canvas size: {last_canvas}", "info")
    log(f"Success evidences: {', '.join(evidences) if evidences else '(none)'}", "info")

    if result == "FAIL" and fail_reason:
        log(f"Fail reason: {fail_reason}", "err")

    # 打印最新日志尾部
    latest = find_latest_log()
    if latest:
        print_log_tail(latest)

    # 清理 HTTP
    try:
        if httpd:
            httpd.shutdown()
    except Exception:
        pass

    return result, fail_reason, last_canvas, evidences


def main():
    parser = argparse.ArgumentParser(description="微信小游戏端到端自动化测试（全自动）")
    parser.add_argument("--project", default=DEFAULT_PROJECT, help="小游戏项目路径")
    parser.add_argument("--cdn-root", default=DEFAULT_CDN_ROOT, help="CDN 根目录")
    parser.add_argument("--cdn-port", type=int, default=8000, help="CDN 端口")
    parser.add_argument("--cli", default=DEFAULT_CLI, help="微信开发者工具 CLI 路径")
    parser.add_argument("--timeout", type=int, default=180, help="每轮日志监控超时秒数")
    parser.add_argument("--max-cycles", type=int, default=3, help="最大重试轮数")
    parser.add_argument("--keep-open", action="store_true", help="测试后保留 IDE")
    args = parser.parse_args()

    start_time = time.time()
    log(f"Starting E2E test (max {args.max_cycles} cycles)", "step")

    final_result = "FAIL"
    final_reason = ""
    final_canvas = ""
    final_evidences = []

    for cycle in range(1, args.max_cycles + 1):
        result, reason, canvas, evidences = run_one_cycle(args, cycle, args.max_cycles)

        if result == "PASS":
            final_result = "PASS"
            final_evidences = evidences
            final_canvas = canvas
            break
        else:
            final_result = result
            final_reason = reason
            final_canvas = canvas
            final_evidences = evidences
            if cycle < args.max_cycles:
                log(f"Cycle {cycle} failed, retrying in 10s...", "info")
                time.sleep(10)

    elapsed = time.time() - start_time

    # 最终结果
    log(f"Final result: {final_result}", "step")
    log(f"Total elapsed: {elapsed:.1f}s", "info")
    log(f"Latest canvas size: {final_canvas}", "info")
    log(f"Success evidences: {', '.join(final_evidences) if final_evidences else '(none)'}", "info")

    print()
    if final_result == "PASS":
        print("\033[32m==========================================")
        print("  TEST PASSED - Game runs successfully!")
        print(f"  Canvas: {final_canvas}")
        print(f"  Evidences: {', '.join(final_evidences)}")
        print(f"  Elapsed: {elapsed:.1f}s")
        print("==========================================\033[0m")
        return 0
    else:
        print("\033[31m==========================================")
        print(f"  TEST FAILED: {final_result}")
        if final_reason:
            print(f"  Reason: {final_reason}")
        print(f"  Canvas: {final_canvas}")
        print(f"  Elapsed: {elapsed:.1f}s")
        print("==========================================\033[0m")
        return 1


if __name__ == "__main__":
    sys.exit(main())
