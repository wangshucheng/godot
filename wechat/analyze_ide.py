"""Analyze IDE screenshot to find the compile button."""
import win32gui, win32con, win32api
from PIL import ImageGrab, Image
import time
import sys

# Find IDE window
results = []
def callback(hwnd, _):
    title = win32gui.GetWindowText(hwnd) or ''
    if '微信开发者工具' not in title:
        return
    if '2048' not in title and 'minigame' not in title.lower():
        return
    rect = win32gui.GetWindowRect(hwnd)
    results.append((hwnd, title, rect))

win32gui.EnumWindows(callback, None)
if not results:
    print('No IDE window found')
    sys.exit(1)

hwnd, title, rect = results[0]
print(f'Window: {title}')
print(f'Rect: {rect}')
print(f'Is visible: {win32gui.IsWindowVisible(hwnd)}')
print(f'Is iconic: {win32gui.IsIconic(hwnd)}')

# Restore if minimized
left, top, right, bottom = rect
if left < -1000 or top < -1000 or win32gui.IsIconic(hwnd):
    print('Window is minimized, restoring...')
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(1)
    win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
    time.sleep(1)
    # Force move to visible position
    win32gui.SetWindowPos(hwnd, 0, 100, 100, 1256, 1000, 0x0040)
    time.sleep(2)
    rect = win32gui.GetWindowRect(hwnd)
    print(f'Restored rect: {rect}')

# Bring to foreground
try:
    win32gui.AllowSetForegroundWindow(-1)
except: pass
try:
    win32api.keybd_event(0x12, 0, 0, 0)
    win32api.keybd_event(0x12, 0, 0x0002, 0)
except: pass
win32gui.SetForegroundWindow(hwnd)
time.sleep(3)

rect = win32gui.GetWindowRect(hwnd)
print(f'Final rect: {rect}')
left, top, right, bottom = rect
w = right - left
h = bottom - top
print(f'Size: {w}x{h}')

if w < 100 or h < 100:
    print('Window still too small, cannot screenshot')
    sys.exit(1)

# Take screenshot
img = ImageGrab.grab(bbox=rect)
out = r'C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_screenshot.png'
img.save(out)
print(f'Screenshot saved: {img.size}')

# Scan the top region for distinct UI elements
px = img.load()
print()
print('=== Toolbar scan (looking for colored regions) ===')

# Scan for green button (compile is often green)
print('Green regions:')
for y in range(0, min(h, 150)):
    in_green = False
    start = 0
    for x in range(w):
        r, g, b = px[x, y][:3]
        is_green = g > 120 and r < 180 and b < 180 and g > r + 15 and g > b + 15
        if is_green and not in_green:
            start = x
            in_green = True
        elif not is_green and in_green:
            if x - start > 15:
                print(f'  y={y}: x={start}-{x} (width={x-start})')
            in_green = False

# Scan for blue button
print()
print('Blue regions:')
for y in range(0, min(h, 150)):
    in_blue = False
    start = 0
    for x in range(w):
        r, g, b = px[x, y][:3]
        is_blue = b > 150 and r < 130 and g > 80 and g < 200 and b > r + 30
        if is_blue and not in_blue:
            start = x
            in_blue = True
        elif not is_blue and in_blue:
            if x - start > 15:
                print(f'  y={y}: x={start}-{x} (width={x-start})')
            in_blue = False

# Scan for any non-background region clusters (buttons/icons)
print()
print('Non-background clusters at y=70-90 (typical toolbar):')
for y in range(60, 100):
    regions = []
    in_content = False
    start = 0
    for x in range(w):
        r, g, b = px[x, y][:3]
        # Background is light gray (240,240,240)-ish
        is_bg = (abs(r - 240) + abs(g - 240) + abs(b - 240)) < 30 or (abs(r - 247) + abs(g - 247) + abs(b - 247)) < 30
        if not is_bg and not in_content:
            start = x
            in_content = True
        elif is_bg and in_content:
            if x - start > 5:
                regions.append((start, x))
            in_content = False
    if regions:
        print(f'  y={y}: {regions[:10]}')
