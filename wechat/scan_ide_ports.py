#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""扫描 wechatdevtools 监听的所有端口"""
import subprocess
import re

out = subprocess.check_output(["tasklist", "/FI", "IMAGENAME eq wechatdevtools.exe", "/FO", "CSV"], text=True, encoding="gbk", errors="ignore")
pids = set(re.findall(r'wechatdevtools\.exe,"(\d+)"', out))
print(f"wechatdevtools PIDs: {sorted(pids)}")

out2 = subprocess.check_output(["netstat", "-ano"], text=True, encoding="gbk", errors="ignore")
print()
print("=== Listening ports ===")
ports_by_pid = {}
for line in out2.splitlines():
    if "LISTENING" not in line.upper():
        continue
    parts = line.split()
    if len(parts) < 5:
        continue
    local = parts[1]
    pid = parts[-1]
    if pid in pids:
        # 提取端口
        m = re.search(r':(\d+)$', local)
        if m:
            port = int(m.group(1))
            ports_by_pid.setdefault(pid, []).append(port)
            print(f"  PID {pid}: {local}  ({line.strip()})")

print()
print("=== Ports in 9400-9500 range (automator default range) ===")
for line in out2.splitlines():
    if "LISTENING" not in line.upper():
        continue
    for p in range(9400, 9500):
        if f":{p} " in line:
            print(f"  {line.strip()}")
            break

print()
print("=== Ports in 9900-10000 range ===")
for line in out2.splitlines():
    if "LISTENING" not in line.upper():
        continue
    for p in range(9900, 10000):
        if f":{p} " in line:
            print(f"  {line.strip()}")
            break
