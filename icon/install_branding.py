#!/usr/bin/env python3
"""
Install Sierra Fox icon assets into browser/branding/unofficial/.

Depends on sierrafox_master.png and sierrafox.icns produced by build_icns.py.
Does not touch Assets.car (compiled Asset Catalog; requires actool to rebuild),
SVG wordmarks, document icons, DMG assets, or Windows VisualElements tiles.
Those are listed at the end of the run and can be addressed separately.
"""

import shutil
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parent
BRANDING = PROJECT / "161.0a1" / "browser" / "branding" / "unofficial"
# ICNS and its underlying master carry the rounded-square + background and
# are used exclusively for the macOS app bundle icon. Every in-UI PNG is
# derived from the transparent circular master instead.
MASTER = HERE / "sierrafox_transparent_master.png"
ICNS = HERE / "sierrafox.icns"

# (destination path relative to BRANDING, width, height)
# When width != height the master is centered on a transparent canvas.
SQUARE_TARGETS = [
    ("default16.png", 16),
    ("default22.png", 22),
    ("default24.png", 24),
    ("default32.png", 32),
    ("default48.png", 48),
    ("default64.png", 64),
    ("default128.png", 128),
    ("default256.png", 256),
    ("content/about-logo.png", 192),
    ("content/about-logo@2x.png", 384),
    ("content/about-logo-private.png", 192),
    ("content/about-logo-private@2x.png", 384),
]

# (destination, canvas_width, canvas_height) — icon is centered, square at min(w,h).
BANNER_TARGETS = [
    ("content/about.png", 300, 236),
]


def main():
    if not MASTER.exists() or not ICNS.exists():
        raise SystemExit("missing sierrafox_master.png or sierrafox.icns — run build_icns.py first")

    master = Image.open(MASTER).convert("RGBA")

    print(f"source: {MASTER.name} ({master.size[0]}x{master.size[1]})")
    print(f"target: {BRANDING}")

    # Square replacements
    for rel, size in SQUARE_TARGETS:
        dst = BRANDING / rel
        resized = master.resize((size, size), Image.LANCZOS)
        resized.save(dst, "PNG", optimize=True)
        print(f"  wrote {rel:48s} {size}x{size}")

    # Banner replacements: center icon on a transparent canvas of the target size.
    for rel, cw, ch in BANNER_TARGETS:
        dst = BRANDING / rel
        side = min(cw, ch)
        resized = master.resize((side, side), Image.LANCZOS)
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(resized, ((cw - side) // 2, (ch - side) // 2), resized)
        canvas.save(dst, "PNG", optimize=True)
        print(f"  wrote {rel:48s} {cw}x{ch} (icon centered at {side}x{side})")

    # firefox.icns — direct swap
    icns_dst = BRANDING / "firefox.icns"
    shutil.copy2(ICNS, icns_dst)
    print(f"  wrote firefox.icns                                   ({icns_dst.stat().st_size // 1024} KB)")

    print()
    print("Not touched (require separate attention):")
    for note in [
        "Assets.car                — compiled Asset Catalog; rebuild with actool from an .xcassets source",
        "content/about-logo.svg    — SVG, kept as-is",
        "content/about-wordmark.svg — text wordmark",
        "content/firefox-wordmark.svg — text wordmark",
        "document.icns / document.ico / content/document_pdf.svg — document-type icons",
        "disk.icns / background.png — DMG installer assets",
        "VisualElements_*.png / PrivateBrowsing_*.png — Windows-only tiles",
    ]:
        print(f"  - {note}")


if __name__ == "__main__":
    main()
