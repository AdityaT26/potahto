#!/usr/bin/env python3
"""Render oled_sim PBM frames like a real SSD1306 and tile them into one PNG.
usage: sheet.py out.png name1 name2 ...   (reads shots/<name>.pbm)"""
import sys
from PIL import Image, ImageDraw, ImageFont
S = 4; PAD = 14; COLS = 3
def load(name):
    t = open(f"shots/{name}.pbm").read().split()
    w, h = int(t[1]), int(t[2]); bits = t[3:]
    img = Image.new("RGB", (w * S, h * S), (8, 10, 14)); d = ImageDraw.Draw(img)
    for y in range(h):
        for x in range(w):
            if bits[y * w + x] == "1": d.rectangle([x*S, y*S, x*S+S-2, y*S+S-2], fill=(150, 215, 255))
    return img
names = sys.argv[2:]
tiles = [load(n) for n in names]
tw, th = tiles[0].size
rows = (len(tiles) + COLS - 1) // COLS
sheet = Image.new("RGB", (COLS * (tw + PAD) + PAD, rows * (th + PAD + 18) + PAD), (40, 40, 44))
d = ImageDraw.Draw(sheet)
for i, (n, t) in enumerate(zip(names, tiles)):
    x = PAD + (i % COLS) * (tw + PAD); y = PAD + (i // COLS) * (th + PAD + 18)
    d.text((x, y), n, fill=(220, 220, 220)); sheet.paste(t, (x, y + 16))
sheet.save(sys.argv[1]); print("wrote", sys.argv[1])
