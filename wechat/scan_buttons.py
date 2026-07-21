"""Scan the IDE screenshot for button colors and text."""
from PIL import Image

img = Image.open(r'C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_screenshot.png')
px = img.load()

# Examine the button area at y=85-105, x=80-220
print('Button area colors (y=85-105, x=80-220, step 5):')
for y in range(85, 110, 2):
    row = []
    for x in range(80, 230, 5):
        r, g, b = px[x, y][:3]
        row.append('({:3d},{:3d},{:3d})'.format(r, g, b))
    print('  y={}: {}'.format(y, ' '.join(row[:15])))

# Look for text-like regions (dark pixels)
print()
print('Dark pixel regions (text/icons) in y=80-120, x=80-500:')
for y in range(80, 125):
    dark_regions = []
    in_dark = False
    start = 0
    for x in range(80, 500):
        r, g, b = px[x, y][:3]
        is_dark = r < 100 and g < 100 and b < 100
        if is_dark and not in_dark:
            start = x
            in_dark = True
        elif not is_dark and in_dark:
            if x - start > 3:
                dark_regions.append((start, x))
            in_dark = False
    if dark_regions:
        print('  y={}: {}'.format(y, dark_regions[:10]))

# Look for colored (non-gray) regions in toolbar
print()
print('Colored (non-gray) regions in y=0-150:')
for y in range(0, 150):
    colored_regions = []
    in_col = False
    start = 0
    col_color = None
    for x in range(0, 600):
        r, g, b = px[x, y][:3]
        # Colored = one channel significantly different from others
        is_colored = (max(r, g, b) - min(r, g, b) > 30) and (max(r, g, b) > 80)
        if is_colored and not in_col:
            start = x
            in_col = True
            col_color = (r, g, b)
        elif not is_colored and in_col:
            if x - start > 8:
                colored_regions.append((start, x, col_color))
            in_col = False
    if colored_regions:
        print('  y={}: {}'.format(y, colored_regions[:5]))
