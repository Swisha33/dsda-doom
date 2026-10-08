#!/usr/bin/env python3
"""Draw the PS Vita LiveArea images for the dsda-doom port.

Original artwork (text only, no id Software logos). LiveArea requires
8-bit indexed PNGs, so every image is quantized to a palette.
Usage: make_livearea.py <out sce_sys dir>
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

out = sys.argv[1]
BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

def font(path, size):
    return ImageFont.truetype(path, size)

def gradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img

def centered(d, w, y, text, f, fill):
    box = d.textbbox((0, 0), text, font=f)
    d.text(((w - (box[2] - box[0])) / 2 - box[0], y), text, font=f, fill=fill)

def save(img, path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.convert("P", palette=Image.ADAPTIVE, colors=256).save(path, optimize=True)

RED_TOP, RED_BOTTOM = (120, 24, 16), (28, 8, 6)
CREAM, AMBER = (240, 226, 210), (245, 166, 52)

# icon0: 128x128
icon = gradient(128, 128, RED_TOP, RED_BOTTOM)
d = ImageDraw.Draw(icon)
d.rectangle([4, 4, 123, 123], outline=AMBER, width=3)
centered(d, 128, 26, "dsda", font(BOLD, 38), CREAM)
centered(d, 128, 72, "DOOM", font(BOLD, 30), AMBER)
save(icon, os.path.join(out, "icon0.png"))

# bg: 840x500
bg = gradient(840, 500, (60, 14, 10), (10, 6, 6))
d = ImageDraw.Draw(bg)
for x in range(-500, 840, 28):
    d.line([(x, 500), (x + 500, 0)], fill=(70, 20, 14), width=2)
centered(d, 840, 150, "dsda-doom", font(BOLD, 86), CREAM)
centered(d, 840, 260, "PS Vita port", font(REG, 34), AMBER)
centered(d, 840, 420, "WADs: ux0:data/dsda-doom/wads", font(REG, 22), (200, 180, 165))
save(bg, os.path.join(out, "livearea/contents/bg.png"))

# startup: 280x158
st = gradient(280, 158, RED_TOP, RED_BOTTOM)
d = ImageDraw.Draw(st)
d.rectangle([3, 3, 276, 154], outline=AMBER, width=3)
centered(d, 280, 38, "dsda-doom", font(BOLD, 38), CREAM)
centered(d, 280, 96, "Start", font(BOLD, 26), AMBER)
save(st, os.path.join(out, "livearea/contents/startup.png"))
