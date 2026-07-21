#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""扫描所有 WeappLog 日志，搜索 canvas/Main._Ready 等关键信息"""
import os
import re
import time

LOG_DIR = r"C:\Users\Administrator\AppData\Local\微信开发者工具\User Data\24d38d8a9569239c3e8419c9a8c32be3\WeappLog"

KEYWORDS = [
    r"canvas\s*=\s*\d+",
    r"Main\._Ready",
    r"WASM instantiation",
    r"Game started successfully",
    r"Game failed to start",
    r"WeChat Boot",
    r"Godot 4\.7",
    r"rAF ticks",
    r"call_indirect",
    r"CANNOT HANDLE COOKIE",
    r"window\.innerWidth",
    r"display_setup",
]

PATTERN = re.compile("|".join(KEYWORDS), re.IGNORECASE)

# 收集所有 .log 文件
all_files = []
for root, dirs, fns in os.walk(LOG_DIR):
    for fn in fns:
        if not fn.endswith(".log"):
            continue
        fp = os.path.join(root, fn)
        try:
            mtime = os.path.getmtime(fp)
            size = os.path.getsize(fp)
            all_files.append((mtime, size, fp))
        except Exception:
            pass

all_files.sort(reverse=True)

print(f"Found {len(all_files)} log files. Scanning top 20 for keywords...\n")

for mtime, size, fp in all_files[:20]:
    t = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(mtime))
    rel = os.path.relpath(fp, LOG_DIR)
    matched = []
    try:
        with open(fp, "r", encoding="utf-8", errors="ignore") as f:
            for i, line in enumerate(f, 1):
                if PATTERN.search(line):
                    matched.append((i, line.rstrip()[:200]))
    except Exception as e:
        print(f"[{t}] {rel} ({size}B) <read failed: {e}>")
        continue

    if matched:
        print(f"[{t}] {rel} ({size}B)  -- {len(matched)} keyword matches:")
        for i, line in matched[:8]:
            print(f"  L{i}: {line}")
        if len(matched) > 8:
            print(f"  ... and {len(matched)-8} more")
        print()
