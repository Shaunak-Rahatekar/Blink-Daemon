from PIL import Image
import sys

img_path = r"C:\Users\SHAUNAK\.gemini\antigravity-ide\brain\a77f6eb5-fb75-4ed5-b168-60250a9de5f1\blink_daemon_logo_1784035247682.png"
ico_path = r"d:\Blink Daemon\icon.ico"

try:
    img = Image.open(img_path)
    icon_sizes = [(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]
    img.save(ico_path, format="ICO", sizes=icon_sizes)
    print("Successfully converted to icon.ico")
except Exception as e:
    print("Error:", e)
    sys.exit(1)
