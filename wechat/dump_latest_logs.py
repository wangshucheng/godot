"""Dump latest log file content."""
import os
import glob
import time

local_appdata = os.environ.get("LOCALAPPDATA", r"C:\Users\Administrator\AppData\Local")
user_data_root = os.path.join(local_appdata, "微信开发者工具", "User Data")
patterns = [
    os.path.join(user_data_root, "*", "WeappLog", "logs"),
]
log_dirs = []
for p in patterns:
    for m in glob.glob(p):
        log_dirs.append(m)

all_files = []
for d in log_dirs:
    for f in os.listdir(d):
        if f.endswith(".log"):
            full = os.path.join(d, f)
            all_files.append((os.path.getmtime(full), full))

all_files.sort()
print(f"Total log files: {len(all_files)}")
# Show 3 most recent
for mtime, path in all_files[-3:]:
    print(f"\n=== {path} ===")
    print(f"Modified: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(mtime))}")
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()
    print(f"Size: {len(content)} chars, {content.count(chr(10))} lines")
    print("--- Last 60 lines ---")
    for line in content.splitlines()[-60:]:
        print(line)
