"""
auto_trigger.py - 通过 SendInput API 发送 Ctrl+B 给 IDE 触发编译
（SendInput 比 keybd_event 更可靠，能注入到输入流）
"""
import os
import sys
import time
import ctypes
import ctypes.wintypes as w

# Find IDE window
import win32gui
import win32con
import win32api

SendInput = ctypes.windll.user32.SendInput
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_UNICODE = 0x0004

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

def send_key_combo(vk_codes, hold_time=0.15):
    """用 SendInput 发送组合键。"""
    inputs = []
    # Press keys in order
    for vk in vk_codes:
        i = INPUT()
        i.type = INPUT_KEYBOARD
        i.ki.wVk = vk
        i.ki.wScan = 0
        i.ki.dwFlags = 0
        i.ki.time = 0
        i.ki.dwExtraInfo = ctypes.pointer(w.ULONG(0))
        inputs.append(i)
    # Send presses
    n = len(inputs)
    arr = (INPUT * n)(*inputs)
    SendInput(n, ctypes.pointer(arr[0]), ctypes.sizeof(INPUT))
    time.sleep(hold_time)
    # Release keys in reverse
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


def find_ide_window():
    results = []
    def cb(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        title = win32gui.GetWindowText(hwnd) or ""
        if "微信开发者工具" in title or "2048" in title:
            results.append((hwnd, title, win32gui.GetWindowRect(hwnd)))
    win32gui.EnumWindows(cb, None)
    return results


def focus_window(hwnd):
    # Restore if minimized
    if win32gui.IsIconic(hwnd):
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(0.3)
    # Move to a known position
    SWP_NOSIZE = 0x0001
    SWP_SHOWWINDOW = 0x0040
    win32gui.SetWindowPos(hwnd, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW)
    time.sleep(0.3)
    # AllowSetForegroundWindow
    ASFW_ANY = -1
    try:
        win32gui.AllowSetForegroundWindow(ASFW_ANY)
    except Exception:
        pass
    # Alt trick
    win32api.keybd_event(0x12, 0, 0, 0)
    win32api.keybd_event(0x12, 0, 0x0002, 0)
    # SetForegroundWindow
    win32gui.SetForegroundWindow(hwnd)
    time.sleep(1.0)
    return win32gui.GetForegroundWindow() == hwnd


def main():
    print("[step] Finding IDE window...")
    windows = find_ide_window()
    if not windows:
        print("[err] No IDE window found")
        return 1
    for hwnd, title, rect in windows:
        print(f"  hwnd={hwnd} title={title!r} rect={rect}")
    # Pick first
    hwnd, title, rect = windows[0]
    print(f"[ok] Target: {title}")
    print(f"     rect={rect}")

    print("[step] Focusing window...")
    if not focus_window(hwnd):
        print("[err] Failed to set foreground")
    else:
        print("[ok] IDE in foreground")
    time.sleep(2)

    # Try multiple shortcut combinations
    VK_CTRL = 0x11
    VK_B = 0x42
    VK_R = 0x52
    VK_F5 = 0x74

    combos = [
        ([VK_CTRL, VK_B], "Ctrl+B (Compile)"),
        ([VK_CTRL, VK_R], "Ctrl+R (Run?)"),
        ([VK_F5], "F5"),
    ]

    for vks, name in combos:
        print(f"[step] Sending {name} via SendInput...")
        try:
            send_key_combo(vks)
            print(f"[ok] Sent")
        except Exception as e:
            print(f"[err] SendInput failed: {e}")
        time.sleep(15)  # Wait for compile effect
        # Check if any new game log appeared
        # (will be checked by caller)
        print(f"[info] Waited 15s after {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
