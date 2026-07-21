#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
完整 E2E 自动化测试（v2）

流程：
1. 杀掉所有 wechatdevtools 进程
2. 清空 WeappLog
3. 启动 HTTP CDN 服务器（serve wasm_pkg）
4. 用 cli.bat open --project <path> --port 9999 打开项目（带 HTTP 服务）
5. 用 cli.bat auto --project <path> --port 9999 --trust-project 启用自动化
6. 等待 WS 端口 9999 可连接
7. 启动 automator_runner.js 子进程，连接 ws://127.0.0.1:9999
   - automator 监听 consoleLog，触发 reLaunch
   - 等待 Main._Ready / canvas=WxH / Game started 等事件
8. 同时并行监控 WeappLog（备份手段）
9. 输出最终报告 + 判定
"""
import os
import re
import sys
import json
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
AUTOMATOR_RUNNER = os.path.join(WS_ROOT, "godot4_7_mono", "wechat", "automator_runner.js")
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
    subprocess.run(
        ["taskkill", "/F", "/IM", "wechatdevtools.exe", "/T"],
        capture_output=True, text=True, encoding="gbk", errors="ignore",
    )
    for _ in range(15):
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
            except Exception:
                pass
    log(f"  cleared {cleared} log files")
    return time.time()


def start_http_server(root, port):
    """启动 HTTP 服务器"""
    handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=root)
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


def monitor_logs_thread(start_after, stop_event, results_dict):
    """后台监控 WeappLog，写入 results_dict（与 automator 结果合并）"""
    seen = {}
    poll = 2.0
    while not stop_event.is_set():
        if os.path.exists(WEAPP_LOG_DIR):
            for root, dirs, files in os.walk(WEAPP_LOG_DIR):
                for fn in files:
                    if not fn.endswith(".log"):
                        continue
                    fp = os.path.join(root, fn)
                    try:
                        mtime = os.path.getmtime(fp)
                        if mtime < start_after:
                            continue
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
                    except Exception:
                        continue
                    for line in chunk.splitlines():
                        for k, p in KEY_PATTERNS.items():
                            m = p.search(line)
                            if not m:
                                continue
                            log(f"  [log:{k}] {line[:200]}")
                            if k == "canvas_size" and results_dict.get("canvas_w") is None:
                                try:
                                    results_dict["canvas_w"] = int(m.group(1))
                                    results_dict["canvas_h"] = int(m.group(2))
                                except Exception:
                                    pass
                            elif k == "diag_canvas":
                                try:
                                    results_dict["diag_canvas_w"] = int(m.group(1))
                                    results_dict["diag_canvas_h"] = int(m.group(2))
                                except Exception:
                                    pass
                            else:
                                results_dict[k] = True
        time.sleep(poll)


def run_automator(timeout_sec=240):
    """启动 automator_runner.js 子进程，返回解析后的 JSON 结果"""
    cmd = ["node", AUTOMATOR_RUNNER, f"ws://127.0.0.1:{CLI_PORT}", str(timeout_sec)]
    log(f"Running automator: {' '.join(cmd)}")
    try:
        r = subprocess.run(
            cmd, capture_output=True, text=True,
            timeout=timeout_sec + 30, encoding="utf-8", errors="ignore",
            cwd=os.path.dirname(AUTOMATOR_RUNNER),
        )
    except subprocess.TimeoutExpired:
        log("automator timed out", "ERROR")
        return {"error": "timeout"}
    except Exception as e:
        log(f"automator exception: {e}", "ERROR")
        return {"error": str(e)}

    if r.stderr:
        for line in r.stderr.splitlines()[-30:]:
            print(f"  [node] {line}", flush=True)
    if r.stdout:
        # 尝试解析 JSON
        try:
            # 找第一个 { 开始的位置
            idx = r.stdout.find("{")
            if idx >= 0:
                return json.loads(r.stdout[idx:])
        except Exception as e:
            log(f"  failed to parse automator JSON: {e}", "WARN")
            log(f"  raw stdout (last 800): {r.stdout[-800:]}")
    return {"error": "no output"}


# ============================================================
# 主流程
# ============================================================
def main():
    print("=" * 70)
    print("WeChat MiniGame E2E Test (v2: cli + automator)")
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

    # Step 4: 用 cli.bat open --project --port 打开项目
    log("=== Step 4: Open project with CLI HTTP server ===")
    r = cli_cmd(["open", "--project", PROJECT_DIR, "--port", str(CLI_PORT)], timeout=120)
    if r is None or r.returncode != 0:
        log("open failed", "ERROR")

    # Step 5: 等待 CLI HTTP 端口就绪
    log(f"=== Step 5: Wait for CLI HTTP port {CLI_PORT} ===")
    if not wait_port("127.0.0.1", CLI_PORT, timeout=60):
        log(f"CLI HTTP port {CLI_PORT} not responding", "ERROR")
        return 1
    log(f"CLI HTTP port {CLI_PORT} is listening")

    # Step 6: 启用自动化
    log("=== Step 6: Enable automation (--trust-project) ===")
    r2 = cli_cmd(["auto", "--project", PROJECT_DIR, "--port", str(CLI_PORT), "--trust-project"], timeout=60)
    # 等待 WS 端口真正可用
    time.sleep(3)

    # Step 7: 启动日志监控线程
    log("=== Step 7: Start log monitor thread ===")
    log_results = {}
    stop_event = threading.Event()
    log_thread = threading.Thread(
        target=monitor_logs_thread,
        args=(start_after, stop_event, log_results),
        daemon=True,
    )
    log_thread.start()

    # Step 8: 运行 automator
    log("=== Step 8: Run automator ===")
    auto_results = run_automator(timeout_sec=240)

    # 停止日志监控
    stop_event.set()
    log_thread.join(timeout=5)

    # Step 9: 合并结果
    log("=== Step 9: Final report ===")
    merged = {
        "boot_start": auto_results.get("boot_start", False) or log_results.get("boot_start", False),
        "wasm_ok": auto_results.get("wasm_ok", False) or log_results.get("wasm_ok", False),
        "main_ready": auto_results.get("main_ready", False) or log_results.get("main_ready", False),
        "game_ok": auto_results.get("game_ok", False) or log_results.get("game_ok", False),
        "game_fail": auto_results.get("game_fail", False) or log_results.get("game_fail", False),
        "compile_err": auto_results.get("compile_err", False) or log_results.get("compile_err", False),
        "canvas_w": auto_results.get("canvas_w") or log_results.get("canvas_w"),
        "canvas_h": auto_results.get("canvas_h") or log_results.get("canvas_h"),
        "diag_canvas_w": auto_results.get("diag_canvas_w") or log_results.get("diag_canvas_w"),
        "diag_canvas_h": auto_results.get("diag_canvas_h") or log_results.get("diag_canvas_h"),
    }
    auto_err = auto_results.get("error")

    print()
    print("=" * 70)
    print("E2E Test Results")
    print("=" * 70)
    print(f"  automator_error : {auto_err}")
    print(f"  boot_start      : {merged['boot_start']}")
    print(f"  wasm_ok         : {merged['wasm_ok']}")
    print(f"  main_ready      : {merged['main_ready']}")
    print(f"  game_ok         : {merged['game_ok']}")
    print(f"  game_fail       : {merged['game_fail']}")
    print(f"  compile_err     : {merged['compile_err']}")
    print(f"  canvas_size     : {merged['canvas_w']}x{merged['canvas_h']}")
    print(f"  diag_canvas     : {merged['diag_canvas_w']}x{merged['diag_canvas_h']}")
    print()
    if auto_results.get("keyLines"):
        print("Automator key lines:")
        for k, line in auto_results["keyLines"][:15]:
            print(f"  [{k}] {line}")
        print()

    # 判定
    canvas_ok = (
        (merged["canvas_w"] is not None and merged["canvas_w"] > 100 and merged["canvas_h"] > 100)
        or (merged["diag_canvas_w"] is not None and merged["diag_canvas_w"] > 100 and merged["diag_canvas_h"] > 100)
    )
    success = (
        merged["main_ready"]
        and not merged["game_fail"]
        and not merged["compile_err"]
        and canvas_ok
    )
    if success:
        print("RESULT: SUCCESS - Game runs and canvas size is valid (>100)")
        return 0
    else:
        print("RESULT: FAIL")
        if auto_err:
            print(f"  Reason: automator error: {auto_err}")
        if merged["game_fail"]:
            print("  Reason: game failed to start")
        if merged["compile_err"]:
            print("  Reason: compile error (call_indirect / cookie)")
        if merged["canvas_w"] is not None and merged["canvas_w"] <= 100:
            print(f"  Reason: canvas too small ({merged['canvas_w']}x{merged['canvas_h']}) - 3x3 bug NOT fixed")
        if not merged["main_ready"]:
            print("  Reason: Main._Ready never called (game never started)")
        return 2


if __name__ == "__main__":
    sys.exit(main())
