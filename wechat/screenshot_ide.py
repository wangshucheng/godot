"""Capture IDE window screenshot."""
import win32gui
import win32con
import time
from PIL import ImageGrab

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

ide = find_ide()
if not ide:
    print("IDE not found")
    exit(1)
hwnd, title, rect = ide
print(f"Found: {title} rect={rect}")

# Restore if minimized
if win32gui.IsIconic(hwnd):
    win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
    time.sleep(0.5)
win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
time.sleep(0.3)
SWP_NOSIZE = 0x0001
SWP_SHOWWINDOW = 0x0040
win32gui.SetWindowPos(hwnd, 0, 50, 50, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW)
time.sleep(1.0)
win32gui.SetForegroundWindow(hwnd)
time.sleep(2)

rect = win32gui.GetWindowRect(hwnd)
print(f"New rect: {rect}")

# Grab screenshot
img = ImageGrab.grab(bbox=rect)
out = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_state.png"
img.save(out)
print(f"Saved: {out} ({img.size})")

# Also crop top-left corner (toolbar area) for analysis
toolbar = img.crop((0, 0, img.size[0], 200))
toolbar_out = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_toolbar2.png"
toolbar.save(toolbar_out)
print(f"Toolbar: {toolbar_out}")

# Scan for button-like regions (top toolbar)
# Look for text-like dark areas in toolbar
print("\nScanning toolbar for distinct colors...")
for y in range(50, 130, 5):
    row_colors = []
    for x in range(0, min(800, img.size[0]), 10):
        p = img.getpixel((x, y))
        if isinstance(p, int):
            p = (p, p, p)
        # Skip pure white and pure black
        if p[0] > 240 and p[1] > 240 and p[2] > 240:
            continue
        if p[0] < 30 and p[1] < 30 and p[2] < 30:
            continue
        row_colors.append((x, p))
    if row_colors:
        print(f"y={y}: {row_colors[:5]}")
