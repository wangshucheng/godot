#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""检查 IDE 当前真实状态：窗口标题、最新日志、自动化端口"""
import os
import sys
import time
import socket
import subprocess
import win32gui

USER_DATA = r"C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3"
WEAPP_LOG = os.path.join(USER_DATA, "WeappLog")
LAUNCH_LOG = os.path.join(WEAPP_LOG, "launch.log")


def list_windows():
    """列出所有可见的、含'微信'或'wechat'字样的窗口"""
    results = []

    def cb(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        title = win32gui.GetWindowText(hwnd) or ""
        if not title:
            return
        if "微信" in title or "wechat" in title.lower() or "devtools" in title.lower():
            results.append((hwnd, title, win32gui.GetWindowRect(hwnd)))

    win32gui.EnumWindows(cb, None)
    return results


def check_port(host, port, timeout=0.5):
    """检查端口是否在监听"""
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(timeout)
    try:
        s.connect((host, port))
        s.close()
        return True
    except Exception:
        return False


def check_port_listen(port):
    """检查本机是否有进程在监听此端口"""
    try:
        out = subprocess.check_output(
            ["netstat", "-ano"], text=True, encoding="gbk", errors="ignore"
        )
        for line in out.splitlines():
            if f":{port} " in line and "LISTENING" in line.upper():
                return line.strip()
    except Exception as e:
        return f"<netstat failed: {e}>"
    return None


def main():
    print("=" * 60)
    print("[1] 可见微信开发者工具相关窗口")
    print("=" * 60)
    wins = list_windows()
    if not wins:
        print("  (无)")
    for hwnd, title, rect in wins:
        print(f"  hwnd={hwnd} title={title!r}")
        print(f"    rect={rect}")

    print()
    print("=" * 60)
    print("[2] 端口监听检查 (9999/3799)")
    print("=" * 60)
    for port in [9999, 3799]:
        listen = check_port_listen(port)
        ok = check_port("127.0.0.1", port)
        print(f"  port {port}: listen={listen!r}, connect={ok}")

    print()
    print("=" * 60)
    print("[3] launch.log 最后 50 行")
    print("=" * 60)
    if os.path.exists(LAUNCH_LOG):
        try:
            with open(LAUNCH_LOG, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
            for line in lines[-50:]:
                print("  " + line.rstrip())
        except Exception as e:
            print(f"  <read failed: {e}>")
    else:
        print(f"  <not found: {LAUNCH_LOG}>")

    print()
    print("=" * 60)
    print("[4] WeappLog 目录下最新 5 个日志文件")
    print("=" * 60)
    if os.path.exists(WEAPP_LOG):
        files = []
        for root, dirs, fns in os.walk(WEAPP_LOG):
            for fn in fns:
                fp = os.path.join(root, fn)
                try:
                    mtime = os.path.getmtime(fp)
                    size = os.path.getsize(fp)
                    files.append((mtime, size, fp))
                except Exception:
                    pass
        files.sort(reverse=True)
        for mtime, size, fp in files[:5]:
            t = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(mtime))
            print(f"  [{t}] {size} bytes  {os.path.basename(fp)}")
    else:
        print(f"  <not found: {WEAPP_LOG}>")


if __name__ == "__main__":
    main()
