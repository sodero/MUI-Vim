#!/usr/bin/env python3
#
# Create palette based versions of the toolbar image strips for AmigaOS 3.
#
# TheBar has to remap every colour of a true colour image to the pens of a
# palette based (AGA) screen.  For the RGBA strips in runtime/icons, with a few
# thousand colours each, this takes more than ten seconds on a 68020 when
# Vim starts.  The palette versions created here load in about a second.
#
# Semi-transparent pixels are blended onto the default MUI background grey,
# pixels with less than half of the maximum opacity become transparent.
#
# Usage: mkpalette.py <runtime/icons> <output directory>
# Requires Pillow.

import glob
import os
import sys

from PIL import Image

BACKGROUND = (170, 170, 170)
COLORS = 31             # plus one transparent entry


def convert(src, dst):
    im = Image.open(src).convert('RGBA')
    w, h = im.size
    pixels = list(im.getdata())
    threshold = max(1, max(p[3] for p in pixels) // 2)

    rgb = Image.new('RGB', (w, h), BACKGROUND)
    opaque = []
    for i, (r, g, b, a) in enumerate(pixels):
        if a >= threshold:
            f = a / 255
            rgb.putpixel((i % w, i // w), tuple(
                round(c * f + bg * (1 - f))
                for c, bg in zip((r, g, b), BACKGROUND)))
            opaque.append(True)
        else:
            opaque.append(False)

    q = rgb.quantize(colors=COLORS, method=Image.Quantize.MEDIANCUT)
    out = Image.new('P', (w, h))
    out.putpalette([0, 0, 0] + q.getpalette()[:COLORS * 3])
    qpixels = list(q.getdata())
    out.putdata([qpixels[i] + 1 if opaque[i] else 0 for i in range(w * h)])
    out.save(dst, transparency=0, optimize=True, bits=5)


def main():
    if len(sys.argv) != 3:
        sys.exit('usage: mkpalette.py <runtime/icons> <output directory>')
    os.makedirs(sys.argv[2], exist_ok=True)
    for src in sorted(glob.glob(os.path.join(sys.argv[1], 'tb_*strip*.png'))):
        dst = os.path.join(sys.argv[2], os.path.basename(src))
        convert(src, dst)
        print(dst)


if __name__ == '__main__':
    main()
