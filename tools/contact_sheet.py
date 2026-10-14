#!/usr/bin/env python3
"""Tile preview PNGs into one image:  contact_sheet.py out.png cols in1.png in2.png ..."""
import sys
from PIL import Image

out, cols, files = sys.argv[1], int(sys.argv[2]), sys.argv[3:]
imgs = [Image.open(f).convert("RGB") for f in files]
w, h = imgs[0].size
rows = (len(imgs) + cols - 1) // cols
sheet = Image.new("RGB", (cols * w, rows * h), (32, 32, 32))
for i, im in enumerate(imgs):
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
sheet.save(out)
