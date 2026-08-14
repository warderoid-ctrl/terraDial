#!/usr/bin/env python3
"""Rasterise the terraPen logo into an LVGL LV_IMG_CF_ALPHA_8BIT bitmap.

Source: theworkisthework/terrapen-identity, Logo/TP-Logo-Animated.svg -- a
single continuous stroked path (fill:none, stroke-width 3 on a 450 viewBox),
which is why this can reuse the same flatten-and-distance-test approach as
gen_lucide_icon.py rather than needing a real fill rasteriser. The animation
in the source (a dot tracing the path) is ignored; we want the finished mark.

Alpha-only output means the mark carries no colour of its own and LVGL tints
it via `img_recolor`. That's what inverts it for us: the source is a black
stroke for print, and on the panel it's drawn in a palette colour on the dark
background, with no image editing involved.

Note the stroke floor: 3 units on a 450 viewBox is well under a pixel once
scaled to ~128px, so a faithful scaling would render the mark invisible. The
stroke is clamped to a minimum width in output pixels instead -- the mark
stays legible, at the cost of being slightly heavier than the print artwork.

Usage: python tools/gen_logo.py [--preview]
Writes: src/display/icon_logo.{h,cpp}
"""
import math
import os
import re
import sys
import urllib.request

SVG_URL = ("https://raw.githubusercontent.com/theworkisthework/"
           "terrapen-identity/main/Logo/TP-Logo-Animated.svg")
VIEWBOX = 450.0
SIZE = 128            # output px, drawn 1:1 on the panel
SUPERSAMPLE = 3
SVG_STROKE = 3.0
MIN_STROKE_PX = 1.5   # see module docstring


def fetch_path_d():
    with urllib.request.urlopen(SVG_URL) as r:
        svg = r.read().decode("utf-8")
    m = re.search(r'<path[^>]*\sd="([^"]+)"', svg, re.S)
    if not m:
        raise SystemExit("couldn't find the logo path in the SVG")
    return m.group(1)


def parse_path(d):
    """Flatten an SVG path to polylines. Supports M/L/H/V/C and relative forms,
    which is everything this particular mark uses."""
    tokens = re.findall(r"([MmLlHhVvCcZz])|(-?\d*\.?\d+(?:e-?\d+)?)", d)
    items = []
    for cmd, num in tokens:
        items.append(cmd if cmd else float(num))

    polys, cur = [], []
    x = y = 0.0
    start = (0.0, 0.0)
    op = None
    i = 0

    def flush():
        if len(cur) > 1:
            polys.append(list(cur))

    while i < len(items):
        if isinstance(items[i], str):
            op = items[i]
            i += 1
            if op in "Zz":
                if cur:
                    cur.append(start)
                flush()
                cur = []
                x, y = start
            continue

        if op in "Mm":
            nx, ny = items[i], items[i + 1]
            i += 2
            x, y = (x + nx, y + ny) if op == "m" else (nx, ny)
            flush()
            cur = [(x, y)]
            start = (x, y)
            op = "l" if op == "m" else "L"  # subsequent pairs are implicit linetos
        elif op in "Ll":
            nx, ny = items[i], items[i + 1]
            i += 2
            x, y = (x + nx, y + ny) if op == "l" else (nx, ny)
            cur.append((x, y))
        elif op in "Hh":
            nx = items[i]
            i += 1
            x = x + nx if op == "h" else nx
            cur.append((x, y))
        elif op in "Vv":
            ny = items[i]
            i += 1
            y = y + ny if op == "v" else ny
            cur.append((x, y))
        elif op in "Cc":
            vals = items[i:i + 6]
            i += 6
            if op == "c":
                x1, y1 = x + vals[0], y + vals[1]
                x2, y2 = x + vals[2], y + vals[3]
                ex, ey = x + vals[4], y + vals[5]
            else:
                x1, y1, x2, y2, ex, ey = vals
            for s in range(1, 13):
                t = s / 12.0
                u = 1 - t
                cur.append((
                    u**3 * x + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t**3 * ex,
                    u**3 * y + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t**3 * ey,
                ))
            x, y = ex, ey
        else:
            raise SystemExit("unsupported path command: %r" % op)

    flush()
    return polys


def dist_to_segment(px, py, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    L = dx * dx + dy * dy
    if L == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def rasterise(polys, size):
    scale = VIEWBOX / size                       # viewBox units per output px
    radius_px = max(MIN_STROKE_PX, SVG_STROKE / scale) / 2.0
    radius = radius_px * scale                   # back into viewBox units

    # Bucket segments by row band so each pixel only tests nearby geometry --
    # brute force over ~1500 segments x 128^2 px x 9 samples is far too slow.
    segs = []
    for poly in polys:
        for i in range(len(poly) - 1):
            segs.append((poly[i], poly[i + 1]))
    bands = {}
    band_h = VIEWBOX / size
    for s in segs:
        lo = int(min(s[0][1], s[1][1]) / band_h) - 1
        hi = int(max(s[0][1], s[1][1]) / band_h) + 1
        for b in range(lo, hi + 1):
            bands.setdefault(b, []).append(s)

    rows = []
    for py in range(size):
        row = []
        for px in range(size):
            hits = 0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    wx = (px + (sx + 0.5) / SUPERSAMPLE) * scale
                    wy = (py + (sy + 0.5) / SUPERSAMPLE) * scale
                    for s in bands.get(int(wy / band_h), ()):
                        if dist_to_segment(wx, wy, s[0], s[1]) <= radius:
                            hits += 1
                            break
            row.append(int(round(255 * hits / (SUPERSAMPLE ** 2))))
        rows.append(row)
    return rows


def main():
    polys = parse_path(fetch_path_d())
    print("parsed %d subpath(s), %d points"
          % (len(polys), sum(len(p) for p in polys)))

    rows = rasterise(polys, SIZE)

    if "--preview" in sys.argv:
        step = max(1, SIZE // 64)
        for y in range(0, SIZE, step * 2):
            print("".join(" .:-=+*#%@"[min(9, rows[y][x] // 26)]
                          for x in range(0, SIZE, step)))

    flat = [v for row in rows for v in row]
    body = ""
    for i in range(0, len(flat), 12):
        body += "    " + " ".join("0x%02x," % v for v in flat[i:i + 12]) + "\n"

    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    base = os.path.join(here, "src", "display", "icon_logo")

    open(base + ".h", "w").write('''#pragma once
#include <lvgl.h>

// The terraPen logo (theworkisthework/terrapen-identity, TP-Logo-Animated.svg)
// rasterised to a %dx%d LV_IMG_CF_ALPHA_8BIT bitmap.
//
// Alpha-only, so it has no colour of its own -- LVGL paints it with the
// object's `img_recolor`. That's how it reads correctly on this panel: the
// source artwork is a black stroke for print, and here it's drawn in a
// palette colour on the dark background without any image editing.
//
// Drawn 1:1. Do NOT scale it with lv_img_set_zoom -- transforming an image
// whose parent also has opacity < 255 sends LVGL down an offscreen-layer
// path that proved unreliable on this board.
//
// Regenerate with: python tools/gen_logo.py
extern const lv_img_dsc_t iconLogo;
''' % (SIZE, SIZE))

    open(base + ".cpp", "w").write('''#include "icon_logo.h"

// Generated -- see icon_logo.h. One byte of alpha per pixel.
static const uint8_t ICON_LOGO_MAP[] = {
%s};

const lv_img_dsc_t iconLogo = {
    {
        LV_IMG_CF_ALPHA_8BIT, // header.cf
        0,                    // header.always_zero
        0,                    // header.reserved
        %d,                   // header.w
        %d,                   // header.h
    },
    sizeof(ICON_LOGO_MAP),
    ICON_LOGO_MAP,
};
''' % (body, SIZE, SIZE))
    print("wrote src/display/icon_logo.{h,cpp}  (%dx%d, %d bytes)"
          % (SIZE, SIZE, SIZE * SIZE))


if __name__ == "__main__":
    main()
