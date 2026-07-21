"""Detailed scan of IDE toolbar to find button boundaries."""
from PIL import Image
import os

img_path = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_state.png"
img = Image.open(img_path)
print(f"Image size: {img.size}")

# Scan y=85 to y=130 (toolbar row)
# Find horizontal runs of distinct colors
print("\n=== Detailed horizontal scan at y=100 ===")
y = 100
prev_color = None
runs = []
start_x = 0
for x in range(img.size[0]):
    p = img.getpixel((x, y))
    if isinstance(p, int):
        p = (p, p, p)
    if p != prev_color:
        if prev_color is not None and runs and runs[-1][0] == prev_color:
            runs[-1] = (prev_color, runs[-1][1], x)
        else:
            runs.append((p, x, x))
        prev_color = p
# Merge adjacent same-color runs
print(f"Found {len(runs)} runs at y={y}")
# Show only runs wider than 10px
print("Runs wider than 10px:")
for color, sx, ex in runs:
    if ex - sx >= 10:
        print(f"  x={sx}-{ex} (w={ex-sx}) color={color}")

# Look for the yellow button area in detail
print("\n=== Yellow button area scan ===")
yellow_xs = []
for x in range(img.size[0]):
    p = img.getpixel((x, 100))
    if isinstance(p, int):
        p = (p, p, p)
    # Yellow: R>200, G>180, B<200
    if p[0] > 200 and p[1] > 180 and p[2] < 200:
        yellow_xs.append(x)
if yellow_xs:
    print(f"Yellow at y=100: x={yellow_xs[0]}-{yellow_xs[-1]} (count={len(yellow_xs)})")

# Scan all y for yellow region
print("\nYellow region boundaries:")
yellow_region = []
for y in range(img.size[1]):
    for x in range(img.size[0]):
        p = img.getpixel((x, y))
        if isinstance(p, int):
            p = (p, p, p)
        if p[0] > 200 and p[1] > 180 and p[2] < 200:
            yellow_region.append((x, y))
            break
if yellow_region:
    xs = [p[0] for p in yellow_region]
    ys = [p[1] for p in yellow_region]
    print(f"Yellow spans: x={min(xs)}-{max(xs)}, y={min(ys)}-{max(ys)}")

# Save cropped yellow region
if yellow_region:
    x_min, x_max = min(xs), max(xs)
    y_min, y_max = min(ys), max(ys)
    # Add padding
    pad = 5
    crop = img.crop((max(0, x_min-pad), max(0, y_min-pad),
                     min(img.size[0], x_max+pad), min(img.size[1], y_max+pad)))
    crop.save(r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\yellow_btn.png")
    print(f"Saved yellow region crop: {crop.size}")

# Also scan for any text region (dark text on light background)
# Save toolbar close-up
toolbar = img.crop((0, 70, 600, 150))
toolbar.save(r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\toolbar_zoom.png")
print(f"Saved zoomed toolbar: {toolbar.size}")

# Now scan for distinct "button-like" rectangles
# Button = rectangular region with consistent non-background color
print("\n=== Finding button rectangles in toolbar ===")
# Sample multiple y values, find all distinct horizontal color bands
all_bands = {}
for y in range(80, 140, 2):
    bands = []
    prev = None
    start = 0
    for x in range(img.size[0]):
        p = img.getpixel((x, y))
        if isinstance(p, int):
            p = (p, p, p)
        if p != prev:
            if prev is not None:
                bands.append((start, x-1, prev))
            prev = p
            start = x
    if prev is not None:
        bands.append((start, img.size[0]-1, prev))
    # Filter to bands wider than 30px and not pure white/black
    for sx, ex, c in bands:
        if ex - sx < 30:
            continue
        if c[0] > 240 and c[1] > 240 and c[2] > 240:
            continue
        if c[0] < 30 and c[1] < 30 and c[2] < 30:
            continue
        key = (sx, ex, c)
        all_bands[key] = all_bands.get(key, 0) + 1

print(f"Found {len(all_bands)} unique bands (width>=30, non-bg)")
# Sort by frequency
sorted_bands = sorted(all_bands.items(), key=lambda x: -x[1])
for (sx, ex, c), cnt in sorted_bands[:20]:
    print(f"  x={sx}-{ex} (w={ex-sx}) color={c} count={cnt}")
