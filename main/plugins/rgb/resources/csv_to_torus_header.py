#!/usr/bin/env python3
"""
Convert a 2D CSV (X,Y) into a C header matching the style of
the provided "torus_points float.h" but for a single slice of points.

Usage:
  python scripts/csv_to_torus_header.py input.csv output.h

The script detects the CSV header (expects columns named X and Y or two columns),
and writes a header with macros for COUNT, SLICES=1, PER_SLICE and a
`const float torus_points[]` array containing alternating x,y floats.
"""
import csv
import sys
import os
from pathlib import Path


def sanitize_guard(name: str) -> str:
    s = ''.join(c if c.isalnum() else '_' for c in name).upper()
    if not s.endswith('_H'):
        s = s + '_H'
    return s


def float_literal(v: float) -> str:
    # Match style: 9 decimal places and trailing 'f'
    return f"{v:.9f}f"


def write_header(points, out_path: Path, var_name='torus_points'):
    count = len(points)
    slices = 1
    per_slice = count
    guard = sanitize_guard(out_path.stem)

    with out_path.open('w', encoding='utf-8') as f:
        f.write('/* Auto-generated from CSV by csv_to_torus_header.py')
        f.write('\n')
        f.write(f'   Count: {count}\n')
        f.write(f'   Slices: {slices}\n')
        f.write(f'   Points per slice: {per_slice}\n')
        f.write('   Stored as: float (32-bit)\n')
        f.write('*/\n\n')
        f.write(f'#ifndef {guard}\n')
        f.write(f'#define {guard}\n\n')
        f.write('#include <stdint.h>\n')
        f.write('#include <stddef.h>\n\n')
        f.write(f'#define TORUS_POINTS_COUNT {count}\n')
        f.write(f'#define TORUS_POINTS_SLICES {slices}\n')
        f.write(f'#define TORUS_POINTS_PER_SLICE {per_slice}\n\n')

        f.write(f'static const float {var_name}[TORUS_POINTS_COUNT][2] = {{\n')

        # write 8 entries per line for readability
        per_line = 8
        for i, (x, y) in enumerate(points):
            s = f'  {{{float_literal(x)}, {float_literal(y)}}}'
            # add comma and spacing
            if i != (count - 1):
                s += ', '
            # newline after per_line entries
            if ((i + 1) % per_line) == 0:
                s += '\n'
            f.write(s)

        # ensure trailing newline
        if (count % per_line) != 0:
            f.write('\n')

        f.write('};\n\n')
        f.write(f'#endif // {guard}\n')


def read_csv_points(csv_path: Path):
    pts = []
    with csv_path.open(newline='') as csvfile:
        reader = csv.reader(csvfile)
        header = next(reader)
        # normalize header
        hdr = [h.strip().upper() for h in header]
        # If header contains X and Y, parse accordingly; otherwise assume two columns
        has_header_xy = False
        if len(hdr) >= 2 and ('X' in hdr[0] or 'X' in hdr[0].upper()) and ('Y' in hdr[1] or 'Y' in hdr[1].upper()):
            has_header_xy = True

        # If header looked like data (numbers), treat that first row as data
        if not has_header_xy:
            # try to parse header row as numbers; if it fails, treat as header and continue
            try:
                x = float(header[0])
                y = float(header[1])
                pts.append((x, y))
            except Exception:
                # header row is non-numeric; skip
                pass

        for row in reader:
            if not row:
                continue
            # take first two columns
            try:
                x = float(row[0])
                y = float(row[1])
            except Exception:
                continue
            pts.append((x, y))
    return pts


def main(argv):
    if len(argv) < 3:
        print('Usage: csv_to_torus_header.py input.csv output.h')
        sys.exit(2)

    in_path = Path(argv[1])
    out_path = Path(argv[2])

    if not in_path.exists():
        print('Input CSV not found:', in_path)
        sys.exit(1)

    points = read_csv_points(in_path)
    if not points:
        print('No points read from CSV')
        sys.exit(1)

    write_header(points, out_path)
    print(f'Wrote {out_path} with {len(points)} points')


if __name__ == '__main__':
    main(sys.argv)
