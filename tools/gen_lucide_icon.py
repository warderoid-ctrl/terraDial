#!/usr/bin/env python3
"""Rasterise a Lucide icon into an LVGL LV_IMG_CF_ALPHA_8BIT C source pair.

LVGL's built-in symbol font has no lamp glyph (and no many-other glyphs), so
icons that matter get traced from the real Lucide SVG instead of substituting
a vaguely-similar symbol. Alpha-only output means the image carries no colour
of its own and LVGL tints it via `img_recolor`, so it can fade and recolour
with the dial exactly like the symbol-font icons beside it.

The path data below is transcribed from the upstream SVG (24x24 viewBox,
stroke-width 2, round caps/joins). Curves are flattened to polylines and
stroked by distance test with 4x supersampling -- enough for a glyph this
small, and it avoids a build-time dependency on a real SVG rasteriser.

Usage:  python tools/gen_lucide_icon.py
Writes: src/display/icon_lightbulb.{h,cpp}

IMPORTANT: pick SIZES that look right drawn 1:1. Do NOT scale these at
runtime with lv_img_set_zoom -- transforming an image whose parent also has
opacity < 255 pushes LVGL down an offscreen-layer path that proved unreliable
on this board (the icon intermittently vanished entirely).
"""
import math
import os

# lucide.dev/icons/lightbulb
#   <path d="M15 14c.2-1 .7-1.7 1.5-2.5 1-.9 1.5-2.2 1.5-3.5A6 6 0 0 0 6 8
#            c0 1 .2 2.2 1.5 3.5.7.7 1.3 1.5 1.5 2.5"/>
#   <path d="M9 18h6"/>
#   <path d="M10 22h4"/>
NAME = "lightbulb"
# One bitmap per size the dial draws the icon at, since an LVGL image can't
# be scaled at runtime here (see the note above). The dial swaps sources the
# same way it swaps font sizes for the symbol icons beside it.
SIZES = [("", 20), ("Large", 28)]
VIEWBOX = 24.0
SUPERSAMPLE = 4
STROKE = 2.0


def bezier(p0, p1, p2, p3, segments=24):
    pts = []
    for i in range(segments + 1):
        t = i / segments
        u = 1 - t
        pts.append((
            u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0],
            u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1],
        ))
    return pts


def build_paths():
    paths = []
    # Right leg: (15,14) up to the dome's right end (18,8)
    paths.append(bezier((15, 14), (15.2, 13.0), (15.7, 12.3), (16.5, 11.5)))
    paths.append(bezier((16.5, 11.5), (17.5, 10.6), (18.0, 9.3), (18.0, 8.0)))
    # Left leg: dome's left end (6,8) down to (9,14)
    paths.append(bezier((6.0, 8.0), (6.0, 9.0), (6.2, 10.2), (7.5, 11.5)))
    paths.append(bezier((7.5, 11.5), (8.2, 12.2), (8.8, 13.0), (9.0, 14.0)))
    # Dome: the A6 6 arc, an upper semicircle centred (12,8) r=6
    dome = []
    for i in range(49):
        a = math.pi * i / 48
        dome.append((12 + 6 * math.cos(a), 8 - 6 * math.sin(a)))
    paths.append(dome)
    # Base lines
    paths.append([(9, 18), (15, 18)])
    paths.append([(10, 22), (14, 22)])
    return paths


def dist_to_segment(px, py, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length_sq = dx * dx + dy * dy
    if length_sq == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_sq))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def rasterise(paths, size):
    radius = STROKE / 2.0
    scale = VIEWBOX / size
    rows = []
    for y in range(size):
        row = []
        for x in range(size):
            hits = 0
            for sy in range(SUPERSAMPLE):
                for sx in range(SUPERSAMPLE):
                    px = (x + (sx + 0.5) / SUPERSAMPLE) * scale
                    py = (y + (sy + 0.5) / SUPERSAMPLE) * scale
                    for poly in paths:
                        covered = False
                        for i in range(len(poly) - 1):
                            if dist_to_segment(px, py, poly[i], poly[i + 1]) <= radius:
                                covered = True
                                break
                        if covered:
                            hits += 1
                            break
            row.append(int(round(255 * hits / (SUPERSAMPLE ** 2))))
        rows.append(row)
    return rows


# ------------------------------------------------------------- output text
HEADER_TEMPLATE = '''#pragma once
#include <lvgl.h>

// Lucide "%s" (lucide.dev/icons/%s), rasterised from the upstream SVG to
// LV_IMG_CF_ALPHA_8BIT bitmaps -- one per size the dial draws it at.
//
// They are alpha-only, so they carry no colour of their own: LVGL paints
// them with the object's `img_recolor` style. That lets the dial tint the
// bulb exactly like the LV_SYMBOL_* text icons it sits alongside (see
// ui_dial.cpp), which a normal colour image could not do.
//
// Each is drawn 1:1 and the dial picks a bitmap rather than resizing one.
// Do NOT scale these with lv_img_set_zoom: transforming an image whose
// parent also has opacity < 255 sends LVGL down an offscreen-layer path
// that proved unreliable here -- the icon intermittently vanished.
//
// Regenerate with: python tools/gen_lucide_icon.py
%s
'''

SOURCE_TEMPLATE = '''#include "icon_%s.h"

// Generated -- see icon_%s.h. One byte of alpha per pixel.

%s
'''

BODY_TEMPLATE = '''static const uint8_t %s_MAP[] = {
%s};

const lv_img_dsc_t %s = {
    {
        LV_IMG_CF_ALPHA_8BIT, // header.cf
        0,                    // header.always_zero
        0,                    // header.reserved
        %d,                   // header.w
        %d,                   // header.h
    },
    sizeof(%s_MAP),
    %s_MAP,
};'''


def main():
    paths = build_paths()
    decls = []
    bodies = []
    for suffix, size in SIZES:
        rows = rasterise(paths, size)
        for row in rows:  # eyeball check
            print("".join(" .:-=+*#%@"[min(9, v // 26)] for v in row))
        print()

        sym = "icon%s%s" % (NAME.capitalize(), suffix)
        macro = ("ICON_%s%s" % (NAME, suffix)).upper()
        flat = [v for row in rows for v in row]
        data = ""
        for i in range(0, len(flat), 12):
            data += "    " + " ".join("0x%02x," % v for v in flat[i:i + 12]) + "\n"

        decls.append("// %dx%d\nextern const lv_img_dsc_t %s;" % (size, size, sym))
        bodies.append(BODY_TEMPLATE % (macro, data, sym, size, size, macro, macro))

    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    base = os.path.join(here, "src", "display", "icon_%s" % NAME)

    open(base + ".h", "w").write(HEADER_TEMPLATE % (NAME, NAME, "\n".join(decls)))
    open(base + ".cpp", "w").write(SOURCE_TEMPLATE % (NAME, NAME, "\n\n".join(bodies)))
    print("wrote %s.{h,cpp}  (%s)" % (
        base, ", ".join("%dx%d" % (sz, sz) for _, sz in SIZES)))


if __name__ == "__main__":
    main()
