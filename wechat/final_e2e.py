"""
final_e2e.py - 最终版端到端测试：监控 WeappLog + 全局搜索 game_diag.log

流程:
1. 杀掉微信开发者工具
2. 清空 WeappLog
3. 启动本地 HTTP CDN 服务器 (端口 8000)
4. cli.bat open --project 打开 IDE (自动编译)
5. 监控 WeappLog 文件 + 全局搜索 game_diag.log 文件
6. 检测关键标志判断成功/失败
"""
import glob
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

import psutil

DEFAULT_PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"
DEFAULT_CDN_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\exports\web_2048"
DEFAULT_CLI = r"D:\software\Tencent\微信web开发者工具\cli.bat"
DEVTOOLS_PROCESS_NAMES = ["wechatdevtools", "wechatdevtools-helper", "WeAppExe", "WeappPlayer", "WeAppPlayer"]
USER_DATA_ROOT = r"C:\Users\Administrator\AppData\Local\微信开发者工具\User Data"


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
    log("Killing DevTools processes", "step")
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
    patterns = [
        os.path.join(USER_DATA_ROOT, "*", "WeappLog", "logs"),
        os.path.join(USER_DATA_ROOT, "*", "Default", "WeappLog", "logs"),
        os.path.join(USER_DATA_ROOT, "Default", "WeappLog", "logs"),
        os.path.join(USER_DATA_ROOT, "WeappLog", "logs"),
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
    log(f"Cleared {total} old log file(s) from {len(log_dirs)} dir(s)", "ok")
    for d in log_dirs:
        log(f"  {d}", "info")


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
        proc = subprocess.Popen(
            [cli_path, "open", "--project", project_path],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        log(f"IDE launch dispatched (launcher PID={proc.pid})", "ok")
        return True
    except Exception as e:
        log(f"Failed to launch IDE: {e}", "err")
        return False


def find_game_diag_logs():
    """搜索所有 game_diag.log 文件（IDE 在 wx.env.USER_DATA_PATH 下创建）"""
    patterns = [
        os.path.join(USER_DATA_ROOT, "**", "game_diag.log"),
        # 也搜全局 AppData，万一路径不在这
        r"C:\Users\Administrator\AppData\Local\微信开发者工具\**\game_diag.log",
    ]
    found = set()
    for p in patterns:
        for m in glob.glob(p, recursive=True):
            found.add(m)
    return list(found)


def monitor_logs(timeout_sec=240):
    """监控 WeappLog + game_diag.log，返回 (result, detail, diag_content)"""
    log(f"Monitoring for {timeout_sec}s", "step")

    deadline = time.time() + timeout_sec
    seen_log_files = set()
    log_dirs = find_log_dirs()
    log(f"Watching {len(log_dirs)} log dir(s)", "info")
    for d in log_dirs:
        log(f"  {d}", "info")

    last_diag_count = 0
    last_check_time = 0
    success_evidence = []
    fail_evidence = []
    diag_content = ""
    last_canvas = ""

    while time.time() < deadline:
        time.sleep(3)
        elapsed = int(time.time() - (deadline - timeout_sec))

        # 1. 扫描 WeappLog
        for d in log_dirs:
            try:
                for f in os.listdir(d):
                    if not f.endswith(".log"):
                        continue
                    full = os.path.join(d, f)
                    if full in seen_log_files:
                        continue
                    seen_log_files.add(full)
                    log(f"[{elapsed}s] NEW LOG: {f}", "info")
                    try:
                        with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                            content = fh.read()
                        # 打印关键行
                        for line in content.splitlines():
                            if any(kw in line for kw in ["[WeChat]", "[C#]", "[Mono]", "canvas=", "Game started",
                                                          "Game failed", "_Ready", "WASM", "error", "Error",
                                                          "FAIL", "PASS"]):
                                log(f"  {line.strip()[:200]}", "log")
                        # 检查成功/失败标志
                        if "[WeChat] Game started successfully" in content or "[C#] Main._Ready" in content:
                            success_evidence.append(f"{f}: Game started/Main._Ready")
                        if "[WeChat] Game failed to start" in content or "WASM instantiation failed" in content:
                            fail_evidence.append(f"{f}: Game failed")
                        if "canvas=" in content:
                            m = re.search(r"canvas=(\d+)x(\d+)", content)
                            if m:
                                w, h = int(m.group(1)), int(m.group(2))
                                last_canvas = f"{w}x{h}"
                                if w <= 10 or h <= 10:
                                    fail_evidence.append(f"{f}: canvas={w}x{h} (3x3 bug)")
                                else:
                                    success_evidence.append(f"{f}: canvas={w}x{h} (normal)")
                    except Exception as e:
                        log(f"  read failed: {e}", "info")
            except Exception:
                continue

        # 2. 扫描 game_diag.log（每 10s 一次）
        if time.time() - last_check_time > 10:
            last_check_time = time.time()
            diag_files = find_game_diag_logs()
            if len(diag_files) > last_diag_count:
                log(f"[{elapsed}s] Found {len(diag_files)} game_diag.log file(s)", "ok")
                for df in diag_files:
                    log(f"  {df}", "info")
                last_diag_count = len(diag_files)
                # 读最新的
                for df in diag_files:
                    try:
                        mtime = os.path.getmtime(df)
                        with open(df, "r", encoding="utf-8", errors="ignore") as fh:
                            content = fh.read()
                        if content != diag_content:
                            diag_content = content
                            log(f"[{elapsed}s] game_diag.log content ({len(content)} bytes, mtime={time.strftime('%H:%M:%S', time.localtime(mtime))}):", "ok")
                            for line in content.splitlines():
                                log(f"  {line.strip()[:200]}", "log")
                            # 检查诊断里的成功/失败标志
                            if "[WeChat Boot] Game started successfully!" in content:
                                success_evidence.append("game_diag.log: Game started")
                            if "[WeChat Boot] Game failed to start" in content:
                                fail_evidence.append("game_diag.log: Game failed")
                            if "[WeChat Boot] index.js load FAILED" in content:
                                fail_evidence.append("game_diag.log: index.js load FAILED")
                            if "[WeChat Boot] adapter load FAILED" in content:
                                fail_evidence.append("game_diag.log: adapter load FAILED")
                            if "[WeChat Boot] Engine start error" in content:
                                fail_evidence.append("game_diag.log: Engine start error")
                    except Exception as e:
                        log(f"  read diag failed: {e}", "info")

        # 判定
        if success_evidence and not fail_evidence:
            log(f"SUCCESS evidence: {success_evidence}", "ok")
            return "SUCCESS", "; ".join(success_evidence), diag_content
        if fail_evidence:
            log(f"FAIL evidence: {fail_evidence}", "err")
            return "FAIL", "; ".join(fail_evidence), diag_content

    return "TIMEOUT", f"last_canvas={last_canvas}, success={success_evidence}, fail={fail_evidence}", diag_content


def main():
    project = DEFAULT_PROJECT
    cdn_root = DEFAULT_CDN_ROOT
    cli = DEFAULT_CLI

    log("=== WeChat MiniGame E2E Test (NO CDN - verify F5 subpackage works) ===", "step")

    # 1. Kill
    kill_devtools()
    kill_port(8000)  # 确保没有 CDN 在跑

    # 2. Clear logs
    clear_weapp_logs()

    # 3. NOT starting CDN - verify F5 subpackage path works standalone
    log("NOT starting CDN server (verifying F5 subpackage path)", "step")

    # 4. Open IDE
    if not open_ide_with_project(cli, project):
        log("IDE launch failed, abort", "err")
        return 1

    # 5. Wait for IDE to fully load + auto-compile
    log("Waiting 30s for IDE to load project + auto-compile...", "step")
    time.sleep(30)

    # 6. Monitor
    result, detail, diag = monitor_logs(timeout_sec=240)

    log(f"\n=== RESULT: {result} ===", "step")
    log(f"Detail: {detail}", "info")
    if diag:
        log(f"Diagnostic file content:", "info")
        for line in diag.splitlines():
            log(f"  {line}", "log")

    if result == "SUCCESS":
        log("GAME FULLY RUNNING WITHOUT CDN! F5 subpackage path works!", "ok")
        return 0
    else:
        log("Game did not fully start", "err")
        return 2


if __name__ == "__main__":
    sys.exit(main())
