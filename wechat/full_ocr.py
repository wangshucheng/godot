"""OCR full IDE image to find what's displayed."""
import subprocess
import pytesseract
from PIL import Image

result = subprocess.run(["where", "tesseract"], capture_output=True, text=True)
tesseract_cmd = result.stdout.strip().split('\n')[0]
pytesseract.pytesseract.tesseract_cmd = tesseract_cmd

img_path = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_state.png"
img = Image.open(img_path)
print(f"Image size: {img.size}")

# Get all text with positions
data = pytesseract.image_to_data(img, lang='chi_sim+eng', output_type=pytesseract.Output.DICT)

# Collect all words and group by line
lines = {}
for i, txt in enumerate(data['text']):
    if not txt.strip():
        continue
    try:
        conf = int(data['conf'][i])
    except:
        continue
    if conf < 30:
        continue
    line_num = data['line_num'][i]
    block_num = data['block_num'][i]
    key = (block_num, line_num)
    if key not in lines:
        lines[key] = []
    x = data['left'][i]
    y = data['top'][i]
    w = data['width'][i]
    h = data['height'][i]
    lines[key].append((x, y, w, h, txt, conf))

print("\n=== All recognized text lines ===")
for key in sorted(lines.keys()):
    parts = lines[key]
    parts.sort(key=lambda p: p[0])
    text = ' '.join(p[4] for p in parts)
    min_x = min(p[0] for p in parts)
    min_y = min(p[1] for p in parts)
    max_x = max(p[0]+p[2] for p in parts)
    max_y = max(p[1]+p[3] for p in parts)
    print(f"  ({min_x},{min_y})-({max_x},{max_y}): {text!r}")

# Save annotated image with boxes
from PIL import ImageDraw, ImageFont
draw_img = img.copy()
draw = ImageDraw.Draw(draw_img)
for key, parts in lines.items():
    for x, y, w, h, txt, conf in parts:
        draw.rectangle([x, y, x+w, y+h], outline='red', width=2)
annotated_path = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_annotated.png"
draw_img.save(annotated_path)
print(f"\nAnnotated image: {annotated_path}")
