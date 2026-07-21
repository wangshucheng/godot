"""Focus IDE window and send Ctrl+B to trigger compile."""
import ctypes
import ctypes.wintypes as w
import time
import sys
import os
import re
import glob
import win32gui
import win32con
import win32api

SendInput = ctypes.windll.user32.SendInput
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002

class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", w.WORD),
                ("wScan", w.WORD),
                ("dwFlags", w.DWORD),
                ("time", w.DWORD),
                ("dwExtraInfo", ctypes.POINTER(w.ULONG))]

class INPUT_UNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT)]

class INPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", w.DWORD),
                ("u", INPUT_UNION)]


def send_key_combo(vk_codes, hold_time=0.2):
    inputs = []
    for vk in vk_codes:
        i = INPUT()
        i.type = INPUT_KEYBOARD
        i.ki.wVk = vk
        i.ki.wScan = 0
        i.ki.dwFlags = 0
        i.ki.time = 0
        i.ki.dwExtraInfo = ctypes.pointer(w.ULONG(0))
        inputs.append(i)
    n = len(inputs)
    arr = (INPUT * n)(*inputs)
    SendInput(n, ctypes.pointer(arr[0]), ctypes.sizeof(INPUT))
    time.sleep(hold_time)
    rels = []
    for vk in reversed(vk_codes):
        i = INPUT()
        i.type = INPUT_KEYBOARD
        i.ki.wVk = vk
        i.ki.wScan = 0
        i.ki.dwFlags = KEYEVENTF_KEYUP
        i.ki.time = 0
        i.ki.dwExtraInfo = ctypes.pointer(w.ULONG(0))
        rels.append(i)
    n = len(rels)
    arr = (INPUT * n)(*rels)
    SendInput(n, ctypes.pointer(arr[0]), ctypes.sizeof(INPUT))


def find_log_dirs():
    local_appdata = os.environ.get("LOCALAPPDATA", r"C:\Users\Administrator\AppData\Local")
    user_data_root = os.path.join(local_appdata, "微信开发者工具", "User Data")
    patterns = [
        os.path.join(user_data_root, "*", "WeappLog", "logs"),
        os.path.join(user_data_root, "Default", "WeappLog", "logs"),
    ]
    found = []
    seen = set()
    for p in patterns:
        for m in glob.glob(p):
            if m not in seen:
                seen.add(m)
                found.append(m)
    return found


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


def focus(hwnd):
    if win32gui.IsIconic(hwnd):
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(0.3)
    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040
    win32gui.SetWindowPos(hwnd, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW)
    time.sleep(0.3)
    ASFW_ANY = -1
    try:
        win32gui.AllowSetForegroundWindow(ASFW_ANY)
    except Exception:
        pass
    win32api.keybd_event(0x12, 0, 0, 0)
    win32api.keybd_event(0x12, 0, 0x0002, 0)
    win32gui.SetForegroundWindow(hwnd)
    time.sleep(1.0)
    return win32gui.GetForegroundWindow() == hwnd


def main():
    print("[step] Finding IDE window...")
    ide = find_ide()
    if not ide:
        print("[err] IDE window not found")
        return 1
    hwnd, title, rect = ide
    print(f"[ok] Found: hwnd={hwnd} title={title!r}")
    print(f"     rect={rect}")

    print("[step] Focusing IDE...")
    if not focus(hwnd):
        print("[err] Could not bring to foreground")
    else:
        print("[ok] IDE in foreground")
    time.sleep(2)

    # Send Ctrl+B multiple times with delays
    VK_CTRL = 0x11
    VK_B = 0x42
    VK_R = 0x52
    VK_F5 = 0x74

    combos = [
        ([VK_CTRL, VK_B], "Ctrl+B"),
        ([VK_CTRL, VK_R], "Ctrl+R"),
        ([VK_F5], "F5"),
    ]

    clear_time = time.time()

    for vks, name in combos:
        print(f"\n[step] Sending {name}...")
        try:
            send_key_combo(vks)
            print(f"[ok] Sent {name}")
        except Exception as e:
            print(f"[err] SendInput failed: {e}")
        print(f"[info] Waiting 25s for {name} to take effect...")
        time.sleep(25)

        # Check logs
        log_dirs = find_log_dirs()
        found_keywords = []
        for d in log_dirs:
            try:
                for f in os.listdir(d):
                    if not f.endswith(".log"):
                        continue
                    full = os.path.join(d, f)
                    if os.path.getmtime(full) < clear_time:
                        continue
                    try:
                        with open(full, "r", encoding="utf-8", errors="ignore") as fh:
                            content = fh.read()
                        for kw in ["[WeChat] Starting Godot", "[WeChat] Game started",
                                   "[C#] Main._Ready", "Godot Engine v4", "WASM instantiation",
                                   "canvas=", "[WeChat] Engine start error", "Game failed"]:
                            if kw in content and kw not in found_keywords:
                                found_keywords.append(kw)
                    except Exception:
                        continue
            except Exception:
                continue
        if found_keywords:
            print(f"[ok] Found after {name}: {found_keywords}")
            # Print lines containing keywords
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
                                if any(kw in line for kw in found_keywords):
                                    print(f"  LOG: {line.rstrip()}")
                except Exception:
                    continue
            return 0
        else:
            print(f"[info] No game logs after {name}")

    print("\n[err] All shortcuts failed to trigger game")
    return 2


if __name__ == "__main__":
    sys.exit(main())
