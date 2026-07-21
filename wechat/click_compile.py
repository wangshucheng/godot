"""Click the compile button in the WeChat DevTools IDE.

Strategy: Restore window -> click at candidate compile button positions -> check logs.
"""
import win32gui, win32con, win32api
import time
import os
import sys
import glob

def log(msg):
    ts = time.strftime('%H:%M:%S')
    print(f'[{ts}] {msg}')

def find_ide_window():
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
    return results[0] if results else None

def find_log_dirs():
    local_appdata = os.environ.get('LOCALAPPDATA', r'C:\Users\Administrator\AppData\Local')
    user_data_root = os.path.join(local_appdata, '微信开发者工具', 'User Data')
    patterns = [
        os.path.join(user_data_root, '*', 'WeappLog', 'logs'),
    ]
    found = []
    seen = set()
    for p in patterns:
        for m in glob.glob(p):
            if m not in seen:
                seen.add(m)
                found.append(m)
    return found

def has_game_logs():
    """Check if any log file contains game runtime keywords."""
    log_dirs = find_log_dirs()
    keywords = ['[WeChat]', '[C#]', '[Mono]', 'canvas=', 'Game started',
                'Main._Ready', 'Godot Engine', 'WASM instantiation']
    for d in log_dirs:
        try:
            for f in os.listdir(d):
                if not f.endswith('.log'):
                    continue
                full = os.path.join(d, f)
                # Only check files modified in the last 60 seconds
                if time.time() - os.path.getmtime(full) > 60:
                    continue
                try:
                    with open(full, 'r', encoding='utf-8', errors='ignore') as fh:
                        content = fh.read()
                    for kw in keywords:
                        if kw in content:
                            return True
                except Exception:
                    continue
        except Exception:
            continue
    return False

# Main
result = find_ide_window()
if not result:
    print('No IDE window found')
    sys.exit(1)

hwnd, title, rect = result
log(f'Window: {title}')
log(f'Rect: {rect}')

# Restore if minimized
left, top, right, bottom = rect
if left < -1000 or top < -1000 or win32gui.IsIconic(hwnd):
    log('Window is minimized, restoring...')
    win32gui.ShowWindow(hwnd, win32con.SW_SHOWNORMAL)
    time.sleep(1)
    win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
    time.sleep(1)
    win32gui.SetWindowPos(hwnd, 0, 100, 100, 1256, 1000, 0x0040)
    time.sleep(2)

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
left, top, right, bottom = rect
log(f'Final rect: {rect}')

# Candidate compile button positions (relative to window top-left)
# Based on screenshot analysis: buttons at x=108-167, y=89-105
# Also try higher positions and sidebar
candidates = [
    (115, 95, 'toolbar btn 1 (x=115,y=95)'),
    (140, 95, 'toolbar btn 2 (x=140,y=95)'),
    (160, 95, 'toolbar btn 3 (x=160,y=95)'),
    (115, 75, 'higher btn (x=115,y=75)'),
    (180, 95, 'toolbar btn 4 (x=180,y=95)'),
    (210, 95, 'toolbar btn 5 (x=210,y=95)'),
    (240, 95, 'toolbar btn 6 (x=240,y=95)'),
    (270, 95, 'toolbar btn 7 (x=270,y=95)'),
    (115, 50, 'tab area (x=115,y=50)'),
    (50, 75, 'sidebar (x=50,y=75)'),
]

for rel_x, rel_y, desc in candidates:
    screen_x = left + rel_x
    screen_y = top + rel_y
    log(f'Clicking {desc} -> screen ({screen_x}, {screen_y})')

    # Move cursor and click
    win32api.SetCursorPos((screen_x, screen_y))
    time.sleep(0.3)
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.1)
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.5)

    # Check for game logs in the next 10 seconds
    found = False
    for wait in range(10):
        time.sleep(1)
        if has_game_logs():
            log(f'GAME LOGS DETECTED after clicking {desc}!')
            found = True
            break

    if found:
        log('Compile triggered successfully!')
        # Wait a bit more and check for success
        time.sleep(30)
        sys.exit(0)
    else:
        log(f'No game logs after {desc}, trying next position...')

log('All candidate positions exhausted, none triggered compile')
sys.exit(1)
