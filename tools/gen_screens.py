#!/usr/bin/env python3
"""Generate SVG illustrations of each terraTouch screen for the README.

These are RENDERS, not screenshots: there's no way to capture the real
panel's framebuffer from a build machine. Everything here is transcribed
from the layout code -- palette hex from include/palette.h, ring radii and
chip sizes from radial_ring.h/ui_dial.cpp, arc angles from the setArcLayout()
calls, hub sizes and font sizes from each ui_*.cpp -- so the proportions and
colours match what the firmware draws. Icons are simplified vector stand-ins
for LVGL's symbol font, which is the main place these diverge.

Keep in sync by hand if a screen's geometry changes.

Usage: python tools/gen_screens.py   ->  docs/screens/*.svg
"""
import math
import os

# --- palette (include/palette.h, ported from terraForge's dark theme) ---
BG_APP = "#1a1a2e"
BG_PANEL = "#16213e"
BG_SECONDARY = "#0f3460"
BORDER = "#5c7a9e"
TEXT = "#e0e0e0"
TEXT_MUTED = "#9ca3af"
TEXT_FAINT = "#818ea5"
ACCENT = "#e94560"
ACCENT_FG = "#ffffff"
ACCENT_SECONDARY = "#60a0ff"
ALERT = "#d12b3f"
GREEN = "#3ddc84"

W = 240
FONT = "Segoe UI, Roboto, Helvetica, Arial, sans-serif"

# --- ring geometry, from radial_ring.h defaults + per-screen overrides ---
DIAL_RADIUS, DIAL_NEAR, DIAL_FAR = 74, 62, 30


def head(parts):
    parts.append(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 240 240" width="240" height="240">'
    )
    # bezel + face: the panel is round, so anything past r=120 is unreachable
    parts.append('<circle cx="120" cy="120" r="119" fill="#0a0a12"/>')
    parts.append('<circle cx="120" cy="120" r="116" fill="%s"/>' % BG_APP)
    parts.append('<clipPath id="face"><circle cx="120" cy="120" r="116"/></clipPath>')
    parts.append('<g clip-path="url(#face)">')


def tail(parts):
    parts.append("</g></svg>")


def text(parts, x, y, s, size=12, fill=TEXT, weight="400", anchor="middle"):
    parts.append(
        '<text x="%g" y="%g" font-family="%s" font-size="%g" font-weight="%s" '
        'fill="%s" text-anchor="%s" dominant-baseline="central">%s</text>'
        % (x, y, FONT, size, weight, fill, anchor, esc(s))
    )


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def circle(parts, cx, cy, r, fill, stroke=None, sw=1, opacity=None):
    op = ' opacity="%g"' % opacity if opacity is not None else ""
    st = ' stroke="%s" stroke-width="%g"' % (stroke, sw) if stroke else ""
    parts.append('<circle cx="%g" cy="%g" r="%g" fill="%s"%s%s/>' % (cx, cy, r, fill, st, op))


def rect(parts, x, y, w, h, r, fill, opacity=None, stroke=None):
    op = ' opacity="%g"' % opacity if opacity is not None else ""
    st = ' stroke="%s" stroke-width="1"' % stroke if stroke else ""
    parts.append(
        '<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s"%s%s/>'
        % (x, y, w, h, r, fill, st, op)
    )


def mix(c1, c2, t):
    """Blend hex colours -- mirrors lv_color_mix(c1, c2, t*255)."""
    a = [int(c1[i : i + 2], 16) for i in (1, 3, 5)]
    b = [int(c2[i : i + 2], 16) for i in (1, 3, 5)]
    return "#%02x%02x%02x" % tuple(int(b[i] + (a[i] - b[i]) * t) for i in range(3))


# ---------------------------------------------------------------- icons
def icon(parts, cx, cy, kind, size, col):
    s = size / 2.0
    lw = max(1.4, size / 11.0)
    st = 'stroke="%s" stroke-width="%g" fill="none" stroke-linecap="round" stroke-linejoin="round"' % (col, lw)
    if kind == "file":
        parts.append('<path d="M%g %g h%g l%g %g v%g h-%g z" %s/>'
                     % (cx - s * .6, cy - s, s * .8, s * .4, s * .4, s * 1.6, s * 1.2, st))
    elif kind == "target":
        circle(parts, cx, cy, s * .8, "none", col, lw)
        parts.append('<path d="M%g %g h%g M%g %g v%g" %s/>' % (cx - s, cy, s * 2, cx, cy - s, s * 2, st))
    elif kind == "pen":
        parts.append('<path d="M%g %g l%g %g M%g %g l%g %g" %s/>'
                     % (cx - s * .7, cy + s * .7, s * 1.4, -s * 1.4, cx - s * .7, cy + s * .7, s * .35, -s * .1, st))
    elif kind == "bulb":  # matches tools/gen_lucide_icon.py
        parts.append('<path d="M%g %g a%g %g 0 1 1 %g 0" %s/>' % (cx - s * .55, cy, s * .55, s * .55, s * 1.1, st))
        parts.append('<path d="M%g %g v%g M%g %g v%g M%g %g h%g M%g %g h%g" %s/>'
                     % (cx - s * .55, cy, s * .35, cx + s * .55, cy, s * .35,
                        cx - s * .35, cy + s * .6, s * .7, cx - s * .22, cy + s * .9, s * .44, st))
    elif kind == "home":
        parts.append('<path d="M%g %g l%g %g l%g %g M%g %g v%g h%g v-%g" %s/>'
                     % (cx - s, cy, s, -s * .85, s, s * .85, cx - s * .7, cy - s * .1,
                        s * 1.1, s * 1.4, s * 1.1, st))
    elif kind == "stop":
        rect(parts, cx - s * .7, cy - s * .7, s * 1.4, s * 1.4, s * .2, col)
    elif kind == "warn":
        parts.append('<path d="M%g %g l%g %g h-%g z" %s/>' % (cx, cy - s, s, s * 1.7, s * 2, st))
        parts.append('<path d="M%g %g v%g" %s/>' % (cx, cy - s * .15, s * .55, st))
        circle(parts, cx, cy + s * .68, lw * .5, col)
    elif kind == "gear":
        circle(parts, cx, cy, s * .42, "none", col, lw)
        for i in range(6):
            a = i * math.pi / 3
            parts.append('<path d="M%g %g L%g %g" %s/>'
                         % (cx + math.cos(a) * s * .62, cy + math.sin(a) * s * .62,
                            cx + math.cos(a) * s, cy + math.sin(a) * s, st))
    elif kind == "wifi":
        for i, rr in enumerate((s * .45, s * .75, s)):
            parts.append('<path d="M%g %g a%g %g 0 0 1 %g 0" %s/>'
                         % (cx - rr, cy + s * .45 - i * 2, rr, rr, rr * 2, st))
        circle(parts, cx, cy + s * .6, lw * .6, col)
    elif kind == "eye":
        parts.append('<path d="M%g %g q%g %g %g 0 q%g %g %g 0" %s/>'
                     % (cx - s, cy, s, -s * .95, s * 2, -s, s * .95, -s * 2, st))
        circle(parts, cx, cy, s * .3, "none", col, lw)
    elif kind == "list":
        for i in (-1, 0, 1):
            parts.append('<path d="M%g %g h%g" %s/>' % (cx - s * .8, cy + i * s * .55, s * 1.6, st))
    elif kind == "drive":
        rect(parts, cx - s, cy - s * .7, s * 2, s * 1.4, s * .25, "none", stroke=col)
        circle(parts, cx, cy, s * .32, col)
    elif kind == "play":
        parts.append('<path d="M%g %g l%g %g l-%g %g z" fill="%s"/>'
                     % (cx - s * .5, cy - s * .7, s * 1.3, s * .7, s * 1.3, s * .7, col))
    elif kind == "pause":
        rect(parts, cx - s * .5, cy - s * .6, s * .35, s * 1.2, 1, col)
        rect(parts, cx + s * .15, cy - s * .6, s * .35, s * 1.2, 1, col)
    elif kind == "back":
        parts.append('<path d="M%g %g l-%g %g l%g %g" %s/>' % (cx + s * .4, cy - s * .6, s * .7, s * .6, s * .7, s * .6, st))


def back_button(parts):
    """The shared bottom-centre back chip (ui_screen_shell.cpp)."""
    circle(parts, 120, 214, 18, BG_SECONDARY)
    icon(parts, 120, 214, "back", 14, TEXT_MUTED)


# ------------------------------------------------------- ring renderers
def full_ring(parts, items, selected, radius=DIAL_RADIUS, near=DIAL_NEAR, far=DIAL_FAR,
              opa_far=110 / 255.0):
    """Home's evenly-spread full circle (RadialRing default mode)."""
    n = len(items)
    step = 360.0 / n
    for i, (kind, alert) in enumerate(items):
        ang = ((i - selected) * step + 180) % 360 - 180
        nearness = 1 - abs(ang) / 180.0
        size = far + (near - far) * nearness
        rad = math.radians(ang)
        cx, cy = 120 + radius * math.sin(rad), 120 - radius * math.cos(rad)
        if alert:
            circle(parts, cx, cy, size / 2, ALERT)
            icon(parts, cx, cy, kind, size * .45, ACCENT_FG)
        else:
            circle(parts, cx, cy, size / 2, mix(ACCENT, BG_SECONDARY, nearness),
                   opacity=opa_far + (1 - opa_far) * nearness)
            icon(parts, cx, cy, kind, size * .45, mix(ACCENT_FG, TEXT_MUTED, nearness))


def arc_ring(parts, kinds, selected, step_deg, half_arc, radius=74, near=56, far=26,
             opa_far=0.0):
    """Jobs/Settings open arc -- bottom left clear for the back button."""
    for i, kind in enumerate(kinds):
        ang = (i - selected) * step_deg
        if abs(ang) >= half_arc:
            continue
        nearness = max(0.0, 1 - abs(ang) / half_arc)
        size = far + (near - far) * nearness
        rad = math.radians(ang)
        cx, cy = 120 + radius * math.sin(rad), 120 - radius * math.cos(rad)
        circle(parts, cx, cy, size / 2, mix(ACCENT, BG_SECONDARY, nearness),
               opacity=opa_far + (1 - opa_far) * nearness)
        icon(parts, cx, cy, kind, size * .45, mix(ACCENT_FG, TEXT_MUTED, nearness))


def hub(parts, size, lines):
    """Centre hub: (text, dy, size, colour, weight) tuples."""
    circle(parts, 120, 120, size / 2, BG_SECONDARY, BORDER, 1)
    for s, dy, sz, col, wt in lines:
        text(parts, 120, 120 + dy, s, sz, col, wt)


# ------------------------------------------------------------- screens
def screen_home():
    p = []
    head(p)
    items = [("file", 0), ("target", 0), ("pen", 0), ("bulb", 0),
             ("home", 0), ("stop", 1), ("warn", 0), ("gear", 0)]
    full_ring(p, items, 0)
    hub(p, 82, [("Jobs", -8, 14, TEXT, "600"), ("IDLE", 12, 12, TEXT_MUTED, "400")])
    tail(p)
    return "home-dial", p


def screen_jobs():
    p = []
    head(p)
    arc_ring(p, ["file"] * 7, 2, 30.0, 132.0)
    hub(p, 96, [("flow_red.gcode", -18, 11, TEXT, "600"),
                ("8.2 MB", 4, 12, TEXT_MUTED, "400")])
    icon(p, 106, 144, "play", 11, ACCENT)
    text(p, 126, 144, "Run", 12, ACCENT, "600")
    back_button(p)
    tail(p)
    return "jobs", p


def screen_jog():
    p = []
    head(p)
    for i, (lbl, sel) in enumerate((("X", 0), ("Y", 1), ("Z", 0))):
        x = 103 + i * 40
        rect(p, x - 17, 32, 34, 26, 13, ACCENT if sel else "none")
        text(p, x, 45, lbl, 14, ACCENT_FG if sel else BORDER, "600")
    text(p, 120, 90, "12.40", 32, TEXT, "600")
    rect(p, 74, 118, 92, 24, 12, BG_SECONDARY)
    text(p, 120, 130, "Set Y zero", 12, TEXT_MUTED)
    for i, (lbl, sel) in enumerate((("0.1", 0), ("1", 1), ("10", 0))):
        x = 74 + i * 46
        rect(p, x, 160, 40, 28, 14, ACCENT if sel else BG_SECONDARY)
        text(p, x + 20, 174, lbl, 12, ACCENT_FG if sel else TEXT_MUTED, "600")
    back_button(p)
    tail(p)
    return "jog", p


def screen_pen():
    p = []
    head(p)
    icon(p, 108, 34, "pen", 14, TEXT_MUTED)
    text(p, 126, 34, "PEN", 12, TEXT_MUTED)
    rect(p, 50, 88, 140, 64, 20, BG_PANEL)
    rect(p, 53, 91, 66, 58, 17, ACCENT)
    text(p, 86, 111, "Pen", 16, ACCENT_FG, "600")
    text(p, 86, 130, "up", 16, ACCENT_FG, "600")
    rect(p, 121, 91, 66, 58, 17, BG_SECONDARY)
    text(p, 154, 111, "Pen", 16, TEXT_MUTED, "600")
    text(p, 154, 130, "down", 16, TEXT_MUTED, "600")
    back_button(p)
    tail(p)
    return "pen", p


def screen_home_confirm():
    p = []
    head(p)
    icon(p, 120, 62, "home", 24, ACCENT)
    text(p, 120, 92, "Clear the bed first", 16, TEXT, "600")
    text(p, 120, 112, "Lift the pen and check the", 12, TEXT_MUTED)
    text(p, 120, 126, "carriage can move freely.", 12, TEXT_MUTED)
    rect(p, 40, 138, 160, 38, 19, ACCENT)
    text(p, 120, 157, "Confirm & home", 14, ACCENT_FG, "600")
    back_button(p)
    tail(p)
    return "home-confirm", p


def screen_lights():
    p = []
    head(p)
    text(p, 120, 52, "LIGHTS", 12, ACCENT_SECONDARY, "600")
    text(p, 120, 74, "Film mode", 12, TEXT_MUTED)
    rect(p, 98, 84, 44, 24, 12, BG_PANEL)
    circle(p, 110, 96, 9, ACCENT_FG)
    text(p, 120, 122, "Rail brightness", 12, TEXT_MUTED)
    rect(p, 30, 132, 180, 10, 5, BG_PANEL)
    rect(p, 30, 132, 180 * .78, 10, 5, ACCENT)
    circle(p, 30 + 180 * .78, 137, 8, ACCENT_FG)
    text(p, 120, 160, "Rail when asleep", 12, TEXT_MUTED)
    for i, (lbl, sel) in enumerate((("On", 1), ("Dim", 0), ("Off", 0))):
        x = 42 + i * 54
        rect(p, x, 170, 48, 26, 13, ACCENT if sel else BG_PANEL)
        text(p, x + 24, 183, lbl, 12, ACCENT_FG if sel else TEXT_MUTED, "600")
    back_button(p)
    tail(p)
    return "lights", p


def screen_settings_ring():
    p = []
    head(p)
    arc_ring(p, ["wifi", "drive", "eye", "list"], 0, 40.0, 132.0,
             near=56, far=34, opa_far=110 / 255.0)
    hub(p, 82, [("Wi-Fi", -8, 14, TEXT, "600"), ("open", 12, 12, ACCENT, "400")])
    back_button(p)
    tail(p)
    return "settings-ring", p


def screen_settings_display():
    p = []
    head(p)
    text(p, 120, 52, "DISPLAY", 12, ACCENT_SECONDARY, "600")
    text(p, 120, 76, "Brightness: 100%", 12, TEXT_MUTED)
    rect(p, 30, 86, 180, 10, 5, BG_PANEL)
    rect(p, 30, 86, 180, 10, 5, ACCENT)
    circle(p, 205, 91, 8, ACCENT_FG)
    text(p, 120, 112, "Invert menu rotation", 12, TEXT_MUTED)
    rect(p, 98, 122, 44, 24, 12, BG_PANEL)
    circle(p, 110, 134, 9, ACCENT_FG)
    text(p, 120, 162, "Sleep after", 12, TEXT_MUTED)
    for i, (lbl, sel) in enumerate((("Never", 0), ("3m", 0), ("5m", 1), ("10m", 0))):
        x = 36 + i * 42
        rect(p, x, 172, 38, 26, 13, ACCENT if sel else BG_PANEL)
        text(p, x + 19, 185, lbl, 11, ACCENT_FG if sel else TEXT_MUTED, "600")
    back_button(p)
    tail(p)
    return "settings-display", p


def screen_job_progress():
    p = []
    head(p)
    circle(p, 120, 120, 106, "none", BG_PANEL, 12)
    pct = 0.42
    a0, a1 = -90, -90 + 360 * pct
    large = 1 if pct > .5 else 0
    x0, y0 = 120 + 106 * math.cos(math.radians(a0)), 120 + 106 * math.sin(math.radians(a0))
    x1, y1 = 120 + 106 * math.cos(math.radians(a1)), 120 + 106 * math.sin(math.radians(a1))
    p.append('<path d="M%g %g A106 106 0 %d 1 %g %g" stroke="%s" stroke-width="12" fill="none"/>'
             % (x0, y0, large, x1, y1, ACCENT))
    text(p, 120, 64, "flow_red.gcode", 12, TEXT_MUTED)
    text(p, 120, 106, "42%", 32, TEXT, "600")
    circle(p, 86, 164, 29, BG_SECONDARY)
    icon(p, 86, 164, "pause", 20, TEXT)
    circle(p, 154, 164, 29, ALERT)
    icon(p, 154, 164, "stop", 16, ACCENT_FG)
    text(p, 120, 206, "pen down", 12, ACCENT_SECONDARY)
    tail(p)
    return "job-progress", p


def screen_estop():
    p = []
    head(p)
    circle(p, 120, 106, 78, ALERT)
    icon(p, 120, 76, "warn", 24, ACCENT_FG)
    text(p, 120, 104, "E-STOP", 18, ACCENT_FG, "700")
    text(p, 120, 128, "Feed hold", 12, ACCENT_FG)
    text(p, 120, 142, "+ soft reset", 12, ACCENT_FG)
    back_button(p)
    tail(p)
    return "estop", p


def screen_alarm():
    p = []
    head(p)
    icon(p, 120, 62, "warn", 24, ACCENT)
    text(p, 120, 92, "Alarm active", 16, TEXT, "600")
    text(p, 120, 112, "Clear the bed, then clear", 12, TEXT_MUTED)
    text(p, 120, 126, "the alarm to continue.", 12, TEXT_MUTED)
    rect(p, 40, 138, 160, 38, 19, ACCENT)
    text(p, 120, 157, "Clear alarm", 14, ACCENT_FG, "600")
    back_button(p)
    tail(p)
    return "alarm-clear", p


def screen_keyboard():
    p = []
    head(p)
    keys = list("abcdefghijklmnopqrstuvwxyz") + ["ABC", "SP", "<x", "OK"]
    sel = 7
    n = len(keys)
    for i, k in enumerate(keys):
        ang = math.radians(-90 + 360.0 * i / n)
        cx, cy = 120 + 98 * math.cos(ang), 120 + 98 * math.sin(ang)
        if i == sel:
            text(p, cx, cy, k, 20, ACCENT, "700")
        else:
            text(p, cx, cy, k, 11, TEXT_MUTED)
    hub(p, 104, [("Password", -30, 12, TEXT_MUTED, "400"),
                 ("*******", -8, 14, TEXT, "600"),
                 ("h", 24, 16, ACCENT, "700")])
    tail(p)
    return "radial-keyboard", p


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "docs", "screens")
    os.makedirs(out, exist_ok=True)
    for fn in (screen_home, screen_jobs, screen_jog, screen_pen, screen_home_confirm,
               screen_lights, screen_settings_ring, screen_settings_display,
               screen_job_progress, screen_estop, screen_alarm, screen_keyboard):
        name, parts = fn()
        path = os.path.join(out, name + ".svg")
        open(path, "w").write("\n".join(parts))
        print("wrote docs/screens/%s.svg" % name)


if __name__ == "__main__":
    main()
