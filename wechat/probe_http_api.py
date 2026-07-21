#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""探测微信开发者工具 IDE 的 HTTP API 端点"""
import urllib.request
import urllib.parse
import json

HOST = "127.0.0.1"
PORT = 9911  # 上次启动 IDE 时用的端口；如果没启动会失败

PROJECT = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\minigame"

# 测试的端点列表（来自 cli.bat 命令名）
endpoints = [
    "/open",
    "/close",
    "/quit",
    "/login",
    "/islogin",
    "/preview",
    "/auto-preview",
    "/auto",
    "/auto-replay",
    "/upload",
    "/build-npm",
    "/cache",
    "/engine",
    "/reset-fileutils",
    "/open-other",
    "/build-ipa",
    "/build-apk",
]

# 项目参数的几种格式
project_params = [
    {},  # 无参数
    {"project": PROJECT},
    {"projectPath": PROJECT},
]

print(f"Probing IDE HTTP API at http://{HOST}:{PORT}/")
print(f"Project: {PROJECT}")
print()

# 先用 GET 测试
print("=== GET requests ===")
for ep in endpoints:
    try:
        r = urllib.request.urlopen(f"http://{HOST}:{PORT}{ep}", timeout=3)
        body = r.read()[:200].decode("utf-8", errors="ignore")
        print(f"  GET {ep}: {r.status} - {body[:100]}")
    except urllib.error.HTTPError as e:
        body = ""
        try:
            body = e.read()[:200].decode("utf-8", errors="ignore")
        except Exception:
            pass
        print(f"  GET {ep}: {e.code} - {body[:100]}")
    except Exception as e:
        print(f"  GET {ep}: ERR {str(e)[:100]}")

print()
print("=== POST requests with empty body ===")
for ep in endpoints:
    try:
        req = urllib.request.Request(f"http://{HOST}:{PORT}{ep}", method="POST")
        req.add_header("Content-Type", "application/json")
        r = urllib.request.urlopen(req, b"{}", timeout=3)
        body = r.read()[:200].decode("utf-8", errors="ignore")
        print(f"  POST {ep}: {r.status} - {body[:100]}")
    except urllib.error.HTTPError as e:
        body = ""
        try:
            body = e.read()[:200].decode("utf-8", errors="ignore")
        except Exception:
            pass
        print(f"  POST {ep}: {e.code} - {body[:100]}")
    except Exception as e:
        print(f"  POST {ep}: ERR {str(e)[:100]}")

print()
print("=== POST requests with project parameter ===")
for ep in endpoints:
    for params in project_params[1:]:  # 跳过空参数
        try:
            req = urllib.request.Request(f"http://{HOST}:{PORT}{ep}", method="POST")
            req.add_header("Content-Type", "application/json")
            data = json.dumps(params).encode()
            r = urllib.request.urlopen(req, data, timeout=3)
            body = r.read()[:200].decode("utf-8", errors="ignore")
            print(f"  POST {ep} ({list(params.keys())}): {r.status} - {body[:100]}")
        except urllib.error.HTTPError as e:
            body = ""
            try:
                body = e.read()[:200].decode("utf-8", errors="ignore")
            except Exception:
                pass
            print(f"  POST {ep} ({list(params.keys())}): {e.code} - {body[:100]}")
        except Exception as e:
            print(f"  POST {ep} ({list(params.keys())}): ERR {str(e)[:100]}")
