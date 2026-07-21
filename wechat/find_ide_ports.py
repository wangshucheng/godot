#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""查找微信开发者工具进程及其监听端口"""
import subprocess
import re

print("=== wechatdevtools.exe processes ===")
out = subprocess.check_output(
    ["tasklist", "/FI", "IMAGENAME eq wechatdevtools.exe", "/FO", "CSV"],
    text=True, encoding="gbk", errors="ignore",
)
print(out)

pids = set(re.findall(r'wechatdevtools\.exe\s+"(\d+)"', out))
print(f"PIDs found: {sorted(pids)}")
print()

print("=== listening TCP ports for these PIDs ===")
out2 = subprocess.check_output(["netstat", "-ano"], text=True, encoding="gbk", errors="ignore")
for line in out2.splitlines():
    upper = line.upper()
    if "LISTENING" not in upper:
        continue
    parts = line.split()
    if len(parts) >= 5 and parts[-1] in pids:
        print(line.strip())

print()
print("=== ALL listening ports (any PID) on 127.0.0.1 ===")
for line in out2.splitlines():
    if "LISTENING" in line.upper() and "127.0.0.1" in line:
        print(line.strip())
