import urllib.request
import json
import sys

# IDE 设置文件中显示 port=9911
PORT = 9911

# 测试各种端点
endpoints = [
    ("/open", "GET"),
    ("/close", "GET"),
    ("/quit", "GET"),
    ("/login", "GET"),
    ("/islogin", "GET"),
    ("/preview", "GET"),
    ("/auto-preview", "GET"),
    ("/auto", "GET"),
    ("/upload", "GET"),
    ("/build-npm", "GET"),
    ("/cache", "GET"),
    ("/engine", "GET"),
    ("/v2/open", "GET"),
    ("/v2/preview", "GET"),
    ("/v2/auto", "GET"),
    ("/v2/build", "GET"),
    ("/v2/compile", "GET"),
    ("/v2/restart", "GET"),
]

print(f"Probing IDE HTTP API on port {PORT}...")
for path, method in endpoints:
    url = f"http://127.0.0.1:{PORT}{path}"
    try:
        req = urllib.request.Request(url, method=method)
        with urllib.request.urlopen(req, timeout=3) as resp:
            body = resp.read(500).decode("utf-8", errors="ignore")
            print(f"  {path:20s} {method:4s} -> {resp.status} {body[:100]}")
    except urllib.error.HTTPError as e:
        body = e.read(500).decode("utf-8", errors="ignore") if e.fp else ""
        print(f"  {path:20s} {method:4s} -> {e.code} {body[:100]}")
    except Exception as e:
        print(f"  {path:20s} {method:4s} -> ERR {type(e).__name__}: {e}")
