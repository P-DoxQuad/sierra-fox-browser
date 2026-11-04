#!/usr/bin/env python3
"""
Build Sierra Fox .icns from the source illustration.

The source PNG has an opaque black background surrounding a rounded-square
illustration. This script measures the rounded square's bounds and corner
radius, replaces the surrounding black with transparency, then renders each
macOS iconset size with high-quality downsampling. iconutil packs the
result into a .icns.

The source file is never modified; it is kept as the reference master.
"""

import os
import shutil
import subprocess
from pathlib import Path
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SRC = HERE / "Sierra Fox Emblem on Navy Gradient.png"
MASTER = HERE / "sierrafox_master.png"
ICONSET = HERE / "sierrafox.iconset"
ICNS = HERE / "sierrafox.icns"

BG_THRESHOLD = 20  # a pixel with every RGB channel below this is background

ICONSET_SIZES = [
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
]


def is_background(rgb):
    return all(c < BG_THRESHOLD for c in rgb[:3])


def measure_bounds(im):
    """Return (left, top, right, bottom) of the non-background region."""
    w, h = im.size
    px = im.load()

    def first_nonbg_row(reverse=False):
        rng = range(h - 1, -1, -1) if reverse else range(h)
        for y in rng:
            for x in range(w):
                if not is_background(px[x, y]):
                    return y
        return None

    def first_nonbg_col(reverse=False):
        rng = range(w - 1, -1, -1) if reverse else range(w)
        for x in rng:
            for y in range(h):
                if not is_background(px[x, y]):
                    return x
        return None

    top = first_nonbg_row()
    bottom = first_nonbg_row(reverse=True)
    left = first_nonbg_col()
    right = first_nonbg_col(reverse=True)
    return left, top, right, bottom


def measure_corner_radius(im, left, top, bottom):
    """Walk the top-left quadrant to measure the corner radius in pixels."""
    px = im.load()
    rows = []
    for y in range(top, bottom):
        for x in range(im.size[0]):
            if not is_background(px[x, y]):
                rows.append((y, x))
                break
    straight_left_x = min(r[1] for r in rows)
    first_straight_y = next(r[0] for r in rows if r[1] == straight_left_x)
    radius = first_straight_y - top
    return radius, straight_left_x


def build_masked(im):
    """Return an RGBA Image with the rounded square isolated against transparency."""
    left, top, right, bottom = measure_bounds(im)
    radius, straight_left_x = measure_corner_radius(im, left, top, bottom)
    print(
        f"  bounds: left={left} top={top} right={right} bottom={bottom}"
        f"  size={right - left + 1}x{bottom - top + 1}"
    )
    print(f"  corner radius: {radius}px ({radius / im.size[0] * 100:.1f}% of width)")

    rgba = im.convert("RGBA")
    mask = Image.new("L", rgba.size, 0)
    draw = ImageDraw.Draw(mask)
    # ImageDraw rounded_rectangle takes an inclusive-exclusive bbox; add 1 to right/bottom.
    draw.rounded_rectangle(
        [(left, top), (right + 1, bottom + 1)],
        radius=radius,
        fill=255,
    )
    rgba.putalpha(mask)
    return rgba


def render_iconset(master):
    if ICONSET.exists():
        shutil.rmtree(ICONSET)
    ICONSET.mkdir()
    for name, size in ICONSET_SIZES:
        resized = master.resize((size, size), Image.LANCZOS)
        out = ICONSET / name
        resized.save(out, "PNG", optimize=True)
        print(f"  wrote {name:28s} {size}x{size}")


def pack_icns():
    if ICNS.exists():
        ICNS.unlink()
    subprocess.run(
        ["iconutil", "-c", "icns", str(ICONSET), "-o", str(ICNS)],
        check=True,
    )
    print(f"  wrote {ICNS.name} ({ICNS.stat().st_size // 1024} KB)")


def main():
    if not SRC.exists():
        raise SystemExit(f"source not found: {SRC}")

    print(f"source: {SRC.name}")
    im = Image.open(SRC)
    print(f"  {im.size[0]}x{im.size[1]} mode={im.mode}")

    print("measuring and masking…")
    master = build_masked(im)

    print(f"writing master: {MASTER.name}")
    master.save(MASTER, "PNG", optimize=True)

    print("rendering iconset…")
    render_iconset(master)

    print("packing icns…")
    pack_icns()


if __name__ == "__main__":
    main()
