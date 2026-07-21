"""Find the compile button in WeChat DevTools IDE using UI Automation."""
import uiautomation as ua
import sys

# Find WeChat DevTools window
root = ua.GetRootControl()
windows = []
for child in root.GetChildren():
    name = child.Name or ''
    if '微信开发者工具' in name or '2048' in name:
        windows.append(child)

if not windows:
    print('No IDE window found')
    sys.exit(1)

win = windows[0]
print(f'Window: {win.Name}')
print(f'Class: {win.ClassName}')
print(f'ControlType: {win.ControlTypeName}')
print(f'Rect: {win.BoundingRectangle}')

# Walk the UI tree
print()
print('=== UI Tree (first 100 controls with names/buttons) ===')

results = []

def walk(ctrl, depth=0, max_depth=10):
    if depth > max_depth:
        return
    try:
        for child in ctrl.GetChildren():
            try:
                ct = child.ControlTypeName
                name = child.Name or ''
                rect = child.BoundingRectangle
                # Capture anything with a name, or any button/toolbar/menu
                if name or ct in ('Button', 'MenuItem', 'ToolBar', 'Menu', 'SplitButton'):
                    results.append((depth, ct, name, rect))
                walk(child, depth + 1, max_depth)
            except Exception:
                pass
    except Exception:
        pass

walk(win)

for depth, ct, name, rect in results[:120]:
    indent = '  ' * depth
    # Truncate long names
    disp_name = name[:60] if name else '(no name)'
    print(f'{indent}[{ct}] "{disp_name}" rect={rect}')

print()
print(f'Total controls found: {len(results)}')

# Look specifically for compile-related buttons
print()
print('=== Compile-related controls ===')
for depth, ct, name, rect in results:
    if any(kw in name for kw in ['编译', 'compile', 'Compile', '预览', 'preview', 'Preview', '运行', 'run', 'Run', '调试', 'debug']):
        print(f'  [{ct}] "{name}" rect={rect}')
