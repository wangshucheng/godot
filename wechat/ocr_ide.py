"""OCR the IDE screenshot to find 'Compile' button location."""
import os
# Find actual tesseract binary
import subprocess
result = subprocess.run(["where", "tesseract"], capture_output=True, text=True)
print("where tesseract:", result.stdout.strip())

# Get tesseract path - the .cmd wrapper
tesseract_cmd = result.stdout.strip().split('\n')[0]
print(f"Using: {tesseract_cmd}")

# Set for pytesseract
import pytesseract
pytesseract.pytesseract.tesseract_cmd = tesseract_cmd

from PIL import Image

img_path = r"C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_state.png"
img = Image.open(img_path)
print(f"Image size: {img.size}")

# Try Chinese + English OCR
print("\n=== Full image OCR (chi_sim+eng) ===")
try:
    text = pytesseract.image_to_string(img, lang='chi_sim+eng')
    print(text[:2000])
except Exception as e:
    print(f"chi_sim+eng failed: {e}")
    print("Falling back to eng only:")
    text = pytesseract.image_to_string(img, lang='eng')
    print(text[:2000])

# OCR just the toolbar region
print("\n=== Toolbar region OCR ===")
toolbar = img.crop((0, 70, 800, 150))
toolbar_text = pytesseract.image_to_string(toolbar, lang='chi_sim+eng')
print("Toolbar text:", repr(toolbar_text))

# Get button positions with bounding boxes
print("\n=== Button positions (image_to_data) ===")
data = pytesseract.image_to_data(img, lang='chi_sim+eng', output_type=pytesseract.Output.DICT)
for i, txt in enumerate(data['text']):
    if txt.strip():
        x = data['left'][i]
        y = data['top'][i]
        w = data['width'][i]
        h = data['height'][i]
        conf = data['conf'][i]
        if int(conf) > 30:  # confidence > 30
            print(f"  '{txt}' at ({x},{y}) size {w}x{h} conf={conf}")
