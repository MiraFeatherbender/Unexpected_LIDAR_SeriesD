#!/usr/bin/env python3
"""Invert a monochrome animated GIF.

Usage:
  python invert_gif.py input.gif               # creates input_inverted.gif
  python invert_gif.py input.gif -o out.gif    # write to out.gif
  python invert_gif.py input.gif --inplace     # overwrite input.gif

This script uses Pillow (install with `pip install Pillow`).
"""
import argparse
import os
import sys
import tempfile
from PIL import Image, ImageOps, ImageSequence


def invert_gif(in_path: str, out_path: str | None = None, inplace: bool = False) -> str:
    im = Image.open(in_path)
    frames = []
    durations = []

    for frame in ImageSequence.Iterator(im):
        durations.append(frame.info.get("duration", 100))
        # Convert to RGB for reliable inversion, preserve alpha if present
        rgba = frame.convert("RGBA")
        rgb = rgba.convert("RGB")
        inverted_rgb = ImageOps.invert(rgb)
        # reattach alpha channel
        r, g, b = inverted_rgb.split()
        a = rgba.split()[3]
        inverted_rgba = Image.merge("RGBA", (r, g, b, a))
        # Convert to palettized frame for GIF saving
        pal = inverted_rgba.convert("P", palette=Image.ADAPTIVE)
        frames.append(pal)

    if out_path is None:
        if inplace:
            # will replace after writing to temp
            out_path = in_path
        else:
            base, ext = os.path.splitext(in_path)
            out_path = f"{base}_inverted{ext}"

    # If writing inplace, write to a temp file first then replace
    if inplace and os.path.abspath(out_path) == os.path.abspath(in_path):
        fd, tmp_name = tempfile.mkstemp(suffix=os.path.splitext(in_path)[1])
        os.close(fd)
        save_path = tmp_name
    else:
        save_path = out_path

    loop = im.info.get("loop", 0)
    frames[0].save(
        save_path,
        save_all=True,
        append_images=frames[1:],
        duration=durations,
        loop=loop,
        disposal=2,
    )

    if save_path != out_path and inplace:
        os.replace(save_path, out_path)

    return out_path


def main():
    parser = argparse.ArgumentParser(description="Invert monochrome animated GIF frames")
    parser.add_argument("input", help="input GIF path")
    parser.add_argument("-o", "--output", help="output GIF path (default: add _inverted)")
    parser.add_argument("--inplace", action="store_true", help="overwrite input file safely")
    args = parser.parse_args()

    try:
        from PIL import Image  # noqa: F401
    except Exception:
        print("Pillow is required. Install with: pip install Pillow", file=sys.stderr)
        sys.exit(2)

    out = invert_gif(args.input, out_path=args.output, inplace=args.inplace)
    print(f"Saved: {out}")


if __name__ == "__main__":
    main()
