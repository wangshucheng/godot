"""List all visible windows with titles."""
import win32gui

def cb(hwnd, _):
    if not win32gui.IsWindowVisible(hwnd):
        return
    title = win32gui.GetWindowText(hwnd) or ""
    if not title:
        return
    cls = win32gui.GetClassName(hwnd)
    rect = win32gui.GetWindowRect(hwnd)
    print(f"hwnd={hwnd} cls={cls!r} title={title!r} rect={rect}")

print("All visible windows with titles:")
win32gui.EnumWindows(cb, None)
