"""Click on the IDE toolbar compile button position.
Based on screenshot analysis: dark button region at x=13-95, y=85-130.
IDE window rect = (50, 50, 1306, 1050), so screen coords = window + relative.
"""
import ctypes
import time
import sys
import os
import glob
import win32gui
import win32con
import win32api

# mouse_event flags
MOUSEEVENTF_MOVE = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004
MOUSEEVENTF_ABSOLUTE = 0x8000


def click_at(x, y, hold_time=0.1):
    """Click at absolute screen coords (x, y)."""
    # Convert to mickey (0-65535 range)
    cx = int(x * 65535 / ctypes.windll.user32.GetSystemMetrics(0))
    cy = int(y * 65535 / ctypes.windll.user32.GetSystemMetrics(1))
    ctypes.windll.user32.SetCursorPos(x, y)
    time.sleep(0.05)
    ctypes.windll.user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(hold_time)
    ctypes.windll.user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.05)


def find_ide():
    target = None
    def cb(hwnd, _):
        nonlocal target
        if not win32gui.IsWindowVisible(hwnd):
            return
        title = win32gui.GetWindowText(hwnd) or ""
        if "微信开发者工具" in title and "2048" in title:
            target = (hwnd, title, win32gui.GetWindowRect(hwnd))
    win32gui.EnumWindows(cb, None)
    return target


def find_log_dirs():
    local_appdata = os.environ.get("LOCALAPPDATA", r"C:\Users\Administrator\AppData\Local")
    user_data_root = os.path.join(local_appdata, "微信开发者工具", "User Data")
    patterns = [
        os.path.join(user_data_root, "*", "WeappLog", "logs"),
    ]
    found = []
    seen = set()
    for p in patterns:
        for m in glob.glob(p):
            if m not in seen:
                seen.add(m)
                found.append(m)
    return found


def check_logs_for_keywords(start_after):
    keywords = ["[WeChat] Starting Godot", "[WeChat] Game started",
                "[C#] Main._Ready", "Godot Engine v4", "WASM instantiation",
                "canvas=", "[WeChat] Engine start error", "Game failed"]
    found = []
    log_dirs = find_log_dirs()
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if not f.endswith(".log"):
                    continue
                full = os.path.join(d, f)
                if os.path.getmtime(full) < start_after:
                    continue
                try:
                    with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                        content = fh.read()
                    for kw in keywords:
                        if kw in content and kw not in found:
                            found.append(kw)
                except Exception:
                    continue
        except Exception:
            continue
    return found


def main():
    print("[step] Finding IDE window...")
    ide = find_ide()
    if not ide:
        print("[err] IDE not found")
        return 1
    hwnd, title, rect = ide
    print(f"[ok] Found: {title}")
    print(f"     rect={rect}")

    # Focus IDE
    if win32gui.IsIconic(hwnd):
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(0.3)
    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040
    win32gui.SetWindowPos(hwnd, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW)
    time.sleep(0.5)
    win32gui.SetForegroundWindow(hwnd)
    time.sleep(1.5)

    rect = win32gui.GetWindowRect(hwnd)
    print(f"     current rect={rect}")
    left, top, right, bottom = rect

    # Based on screenshot analysis:
    # IDE toolbar at y=85-130 (relative to window)
    # Dark button area at x=13-95 (relative to window content)
    # But window has title bar (~30px) and borders
    # Actual content starts around (left+8, top+60) or so

    # Try multiple candidate button positions (in screen coords)
    candidates = [
        # (x, y, description) - based on screenshot analysis
        (left + 50, top + 110, "toolbar-left-1 (50,110)"),
        (left + 80, top + 110, "toolbar-left-2 (80,110)"),
        (left + 110, top + 110, "toolbar-left-3 (110,110)"),
        (left + 140, top + 110, "toolbar-mid-1 (140,110)"),
        (left + 170, top + 110, "toolbar-mid-2 (170,110)"),
        (left + 200, top + 110, "toolbar-mid-3 (200,110)"),
        (left + 50, top + 90, "toolbar-upper-1 (50,90)"),
        (left + 100, top + 90, "toolbar-upper-2 (100,90)"),
        (left + 150, top + 90, "toolbar-upper-3 (150,90)"),
        # Common compile button positions in WeChat DevTools
        (left + 30, top + 70, "compile-btn-typical-1"),
        (left + 60, top + 70, "compile-btn-typical-2"),
        (left + 90, top + 70, "compile-btn-typical-3"),
    ]

    clear_time = time.time()

    for x, y, desc in candidates:
        print(f"\n[step] Clicking {desc} at screen ({x},{y})...")
        try:
            click_at(x, y)
            print(f"[ok] Clicked")
        except Exception as e:
            print(f"[err] Click failed: {e}")
            continue

        # Wait 15s for effect
        print(f"[info] Waiting 15s for effect...")
        time.sleep(15)

        # Check logs
        found = check_logs_for_keywords(clear_time)
        if found:
            print(f"[ok] Game logs detected: {found}")
            # Print full content
            log_dirs = find_log_dirs()
            for d in log_dirs:
                try:
                    for f in os.listdir(d):
                        if not f.endswith(".log"):
                            continue
                        full = os.path.join(d, f)
                        if os.path.getmtime(full) < clear_time:
                            continue
                        with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                            for line in fh:
                                if any(kw in line for kw in found):
                                    print(f"  LOG: {line.rstrip()}")
                except Exception:
                    continue
            return 0
        else:
            print(f"[info] No game logs after click at ({x},{y})")

    print("\n[err] All click positions failed")
    return 2


if __name__ == "__main__":
    sys.exit(main())
