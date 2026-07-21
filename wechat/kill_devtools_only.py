"""Kill all DevTools processes."""
import psutil
import time

NAMES = ["wechatdevtools", "wechatdevtools-helper", "WeAppExe", "WeappPlayer", "WeAppPlayer"]
killed = 0
for proc in psutil.process_iter(["pid", "name"]):
    try:
        pname = proc.info["name"] or ""
        if pname.lower().endswith(".exe"):
            base = pname[:-4].lower()
            if base in [n.lower() for n in NAMES]:
                print(f"Killing {proc.info['pid']} {pname}")
                proc.kill()
                killed += 1
    except Exception:
        continue
print(f"Killed {killed} processes")
time.sleep(3)
