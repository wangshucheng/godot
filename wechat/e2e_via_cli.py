#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
完整 E2E 自动化测试（基于微信开发者工具 CLI HTTP 接口）

流程：
1. 杀掉所有 wechatdevtools 进程
2. 清空 WeappLog（便于后续定位新日志）
3. 启动 HTTP CDN 服务器（serve wasm_pkg）
4. 用 cli.bat open --project <path> --port 9999 打开项目
5. 等待 IDE 启动 + HTTP 端口就绪
6. 用 cli.bat auto-preview 触发编译/预览（或 cli.bat preview）
7. 监控 WeappLog，等待 [C#] Main._Ready / canvas=WxH 关键日志
8. 校验 canvas 尺寸是否 >100（验证 3x3 修复）
9. 输出最终报告
"""
import os
import re
import sys
import time
import socket
import shutil
import subprocess
import threading
import http.server
import socketserver
import functools

# ============================================================
# 配置
# ============================================================
WS_ROOT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
PROJECT_DIR = os.path.join(WS_ROOT, "godot4_7_mono", "wechat", "minigame")
CDN_ROOT = os.path.join(WS_ROOT, "godot4_7_mono", "exports", "web_2048")
CLI_BAT = r"D:\software\Tencent\微信web开发者工具\cli.bat"
WEAPP_LOG_DIR = r"C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappLog"
CLI_PORT = 9999
CDN_PORT = 8000

# 关键关键字（命中任一即认为游戏开始运行/失败）
KEY_PATTERNS = {
    "main_ready": re.compile(r"Main\._?Ready", re.IGNORECASE),
    "canvas_size": re.compile(r"canvas\s*=\s*(\d+)\s*[x×]\s*(\d+)", re.IGNORECASE),
    "wasm_ok": re.compile(r"WASM instantiation succeeded", re.IGNORECASE),
    "game_ok": re.compile(r"Game started successfully", re.IGNORECASE),
    "game_fail": re.compile(r"Game failed to start|Engine start error", re.IGNORECASE),
    "compile_err": re.compile(r"CompileError|call_indirect|CANNOT HANDLE COOKIE", re.IGNORECASE),
    "boot_start": re.compile(r"WeChat Boot|Starting Godot 4\.7", re.IGNORECASE),
    "diag_canvas": re.compile(r"\[WeChat Diag\][^\n]*canvas\s*=\s*(\d+)\s*[x×]\s*(\d+)", re.IGNORECASE),
}

# ============================================================
# 工具函数
# ============================================================
def log(msg, level="INFO"):
    t = time.strftime("%H:%M:%S")
    print(f"[{t}] [{level}] {msg}", flush=True)


def kill_devtools():
    """杀掉所有 wechatdevtools 进程"""
    log("Killing all wechatdevtools processes...")
    # 用 taskkill 强制杀
    subprocess.run(
        ["taskkill", "/F", "/IM", "wechatdevtools.exe", "/T"],
        capture_output=True, text=True, encoding="gbk", errors="ignore",
    )
    # 等待进程退出
    for _ in range(10):
        time.sleep(1)
        out = subprocess.check_output(
            ["tasklist", "/FI", "IMAGENAME eq wechatdevtools.exe", "/FO", "CSV"],
            text=True, encoding="gbk", errors="ignore",
        )
        if "wechatdevtools.exe" not in out:
            log("All wechatdevtools processes terminated.")
            return True
    log("Some wechatdevtools processes still alive", "WARN")
    return False


def clear_weapp_logs():
    """清空 WeappLog 目录下的所有日志文件，返回清空时间戳"""
    log(f"Clearing logs in {WEAPP_LOG_DIR} ...")
    cleared = 0
    if not os.path.exists(WEAPP_LOG_DIR):
        return time.time()
    for root, dirs, files in os.walk(WEAPP_LOG_DIR):
        for fn in files:
            if not fn.endswith(".log"):
                continue
            fp = os.path.join(root, fn)
            try:
                os.unlink(fp)
                cleared += 1
            except Exception as e:
                log(f"  failed to delete {fp}: {e}", "WARN")
    log(f"  cleared {cleared} log files")
    return time.time()


def start_http_server(root, port):
    """启动 HTTP 服务器，返回 server 对象"""
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=root)
    # ThreadingHTTPServer
    socketserver.TCPServer.allow_reuse_address = True
    httpd = socketserver.ThreadingTCPServer(("0.0.0.0", port), handler)
    httpd.daemon_threads = True
    t = threading.Thread(target=httpd.serve_forever, daemon=True)
    t.start()
    log(f"HTTP CDN server started at http://0.0.0.0:{port}/ serving {root}")
    return httpd


def wait_port(host, port, timeout=60, poll=0.5):
    """等待端口可连接"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(poll)
            s.connect((host, port))
            s.close()
            return True
        except Exception:
            time.sleep(poll)
    return False


def cli_cmd(args, timeout=120):
    """执行 cli.bat 命令（同步阻塞，捕获输出）"""
    cmd = [CLI_BAT] + args
    log(f"CLI: {' '.join(args)}")
    try:
        r = subprocess.run(
            cmd, capture_output=True, text=True,
            timeout=timeout, encoding="utf-8", errors="ignore",
        )
        if r.stdout:
            log(f"  stdout (last 500): ...{r.stdout[-500:]}")
        if r.stderr:
            log(f"  stderr (last 500): ...{r.stderr[-500:]}", "WARN")
        log(f"  exit code: {r.returncode}")
        return r
    except subprocess.TimeoutExpired:
        log(f"  TIMEOUT after {timeout}s", "ERROR")
        return None
    except Exception as e:
        log(f"  exception: {e}", "ERROR")
        return None


def find_new_log_files(start_after, max_wait=180, poll=2.0):
    """轮询 WeappLog 目录，返回 mtime >= start_after 的所有 .log 文件"""
    deadline = time.time() + max_wait
    found = []
    while time.time() < deadline:
        found = []
        if os.path.exists(WEAPP_LOG_DIR):
            for root, dirs, files in os.walk(WEAPP_LOG_DIR):
                for fn in files:
                    if not fn.endswith(".log"):
                        continue
                    fp = os.path.join(root, fn)
                    try:
                        mtime = os.path.getmtime(fp)
                        if mtime >= start_after:
                            found.append((mtime, fp))
                    except Exception:
                        pass
        if found:
            found.sort()
            return found
        time.sleep(poll)
    return found


def monitor_logs(start_after, timeout_sec=240, poll=2.0):
    """监控新日志，直到命中关键事件或超时"""
    log(f"Monitoring logs (timeout={timeout_sec}s)...")
    deadline = time.time() + timeout_sec
    seen = {}    # fp -> last_read_offset
    results = {
        "main_ready": False,
        "wasm_ok": False,
        "game_ok": False,
        "game_fail": False,
        "compile_err": False,
        "boot_start": False,
        "canvas_w": None,
        "canvas_h": None,
        "diag_canvas_w": None,
        "diag_canvas_h": None,
        "first_canvas_line": None,
        "key_lines": [],   # 所有匹配的关键行
    }

    while time.time() < deadline:
        # 枚举所有新日志
        new_files = []
        if os.path.exists(WEAPP_LOG_DIR):
            for root, dirs, files in os.walk(WEAPP_LOG_DIR):
                for fn in files:
                    if not fn.endswith(".log"):
                        continue
                    fp = os.path.join(root, fn)
                    try:
                        mtime = os.path.getmtime(fp)
                        if mtime >= start_after:
                            new_files.append((mtime, fp))
                    except Exception:
                        pass
        new_files.sort()

        for mtime, fp in new_files:
            try:
                size = os.path.getsize(fp)
            except Exception:
                continue
            offset = seen.get(fp, 0)
            if size <= offset:
                continue
            try:
                with open(fp, "r", encoding="utf-8", errors="ignore") as f:
                    f.seek(offset)
                    chunk = f.read()
                    seen[fp] = f.tell()
            except Exception as e:
                continue

            for line in chunk.splitlines():
                # 检查所有 pattern
                if KEY_PATTERNS["main_ready"].search(line):
                    results["main_ready"] = True
                    results["key_lines"].append(("main_ready", line[:200]))
                if KEY_PATTERNS["wasm_ok"].search(line):
                    results["wasm_ok"] = True
                    results["key_lines"].append(("wasm_ok", line[:200]))
                if KEY_PATTERNS["game_ok"].search(line):
                    results["game_ok"] = True
                    results["key_lines"].append(("game_ok", line[:200]))
                if KEY_PATTERNS["game_fail"].search(line):
                    results["game_fail"] = True
                    results["key_lines"].append(("game_fail", line[:200]))
                if KEY_PATTERNS["compile_err"].search(line):
                    results["compile_err"] = True
                    results["key_lines"].append(("compile_err", line[:200]))
                if KEY_PATTERNS["boot_start"].search(line):
                    results["boot_start"] = True
                    results["key_lines"].append(("boot_start", line[:200]))
                m = KEY_PATTERNS["canvas_size"].search(line)
                if m and results["canvas_w"] is None:
                    try:
                        results["canvas_w"] = int(m.group(1))
                        results["canvas_h"] = int(m.group(2))
                        results["first_canvas_line"] = line[:200]
                    except Exception:
                        pass
                m2 = KEY_PATTERNS["diag_canvas"].search(line)
                if m2:
                    try:
                        results["diag_canvas_w"] = int(m2.group(1))
                        results["diag_canvas_h"] = int(m2.group(2))
                        results["key_lines"].append(("diag_canvas", line[:200]))
                    except Exception:
                        pass

        # 终止条件：游戏成功或失败都退出
        if results["game_ok"] or results["game_fail"] or results["compile_err"]:
            log(f"Termination condition hit: "
                f"game_ok={results['game_ok']} game_fail={results['game_fail']} "
                f"compile_err={results['compile_err']}")
            break
        # 或者 canvas 已经出现 + Main._Ready 也已经出现
        if results["main_ready"] and (results["canvas_w"] is not None or results["diag_canvas_w"] is not None):
            log(f"Main._Ready + canvas size both detected, finishing early")
            break

        time.sleep(poll)

    return results


# ============================================================
# 主流程
# ============================================================
def main():
    print("=" * 70)
    print("WeChat MiniGame E2E Test via CLI HTTP")
    print("=" * 70)
    print(f"Project: {PROJECT_DIR}")
    print(f"CDN root: {CDN_ROOT}")
    print(f"CLI port: {CLI_PORT}, CDN port: {CDN_PORT}")
    print()

    # Step 1: 杀掉 IDE
    log("=== Step 1: Kill existing IDE ===")
    kill_devtools()
    time.sleep(2)

    # Step 2: 清空日志
    log("=== Step 2: Clear logs ===")
    start_after = clear_weapp_logs()

    # Step 3: 启动 CDN
    log("=== Step 3: Start CDN HTTP server ===")
    if not os.path.exists(CDN_ROOT):
        log(f"CDN root not found: {CDN_ROOT}", "ERROR")
        return 1
    try:
        cdn_server = start_http_server(CDN_ROOT, CDN_PORT)
    except Exception as e:
        log(f"Failed to start CDN: {e}", "ERROR")
        return 1

    # Step 4: 用 cli.bat open --project --port 打开项目并启动 HTTP 服务
    log("=== Step 4: Open project with CLI HTTP server ===")
    r = cli_cmd(["open", "--project", PROJECT_DIR, "--port", str(CLI_PORT)], timeout=60)
    if r is None or r.returncode != 0:
        log("open failed", "ERROR")

    # Step 5: 等待 CLI HTTP 端口就绪
    log(f"=== Step 5: Wait for CLI HTTP port {CLI_PORT} ===")
    if not wait_port("127.0.0.1", CLI_PORT, timeout=60):
        log(f"CLI HTTP port {CLI_PORT} not responding", "ERROR")
        # 但不放弃，继续尝试 preview
    else:
        log(f"CLI HTTP port {CLI_PORT} is listening")

    # Step 6: 触发 auto-preview（会触发编译）
    log("=== Step 6: Trigger auto-preview (compile) ===")
    r2 = cli_cmd(["auto-preview", "--project", PROJECT_DIR, "--port", str(CLI_PORT)], timeout=120)

    # Step 7: 监控日志
    log("=== Step 7: Monitor logs for game runtime ===")
    results = monitor_logs(start_after=start_after, timeout_sec=240, poll=2.0)

    # Step 8: 报告
    log("=== Step 8: Final report ===")
    print()
    print("=" * 70)
    print("E2E Test Results")
    print("=" * 70)
    print(f"  boot_start   : {results['boot_start']}")
    print(f"  wasm_ok      : {results['wasm_ok']}")
    print(f"  main_ready   : {results['main_ready']}")
    print(f"  game_ok      : {results['game_ok']}")
    print(f"  game_fail    : {results['game_fail']}")
    print(f"  compile_err  : {results['compile_err']}")
    print(f"  canvas_size  : {results['canvas_w']}x{results['canvas_h']}")
    print(f"  diag_canvas  : {results['diag_canvas_w']}x{results['diag_canvas_h']}")
    print()
    if results["key_lines"]:
        print("Key log lines:")
        for kind, line in results["key_lines"]:
            print(f"  [{kind}] {line}")
    print()

    # 判定
    success = (
        results["main_ready"]
        and not results["game_fail"]
        and not results["compile_err"]
        and (
            (results["canvas_w"] is not None and results["canvas_w"] > 100 and results["canvas_h"] > 100)
            or (results["diag_canvas_w"] is not None and results["diag_canvas_w"] > 100)
        )
    )
    if success:
        print("RESULT: SUCCESS - Game runs and canvas size is valid (>100)")
        return 0
    else:
        print("RESULT: FAIL - See above for details")
        if results["game_fail"]:
            print("  Reason: game failed to start")
        if results["compile_err"]:
            print("  Reason: compile error (call_indirect / cookie)")
        if results["canvas_w"] is not None and results["canvas_w"] <= 100:
            print(f"  Reason: canvas too small ({results['canvas_w']}x{results['canvas_h']}) - 3x3 bug NOT fixed")
        if not results["main_ready"]:
            print("  Reason: Main._Ready never called (game never started)")
        return 2


if __name__ == "__main__":
    sys.exit(main())
