#pragma once
#include <lvgl.h>

// "TerraPen Dial UI" mockup skin (Radial dial menu mockups/design_handoff_
// radial_dial_ui/) -- best-effort OKLCH -> sRGB conversions of the
// mockup's design tokens (deep graphite-lavender bg, lavender primary
// accent, warm-peach secondary accent). Exact fidelity depends on the real
// IPS panel, not a monitor -- nudge these after flashing if something
// looks off, rather than treating the hex values as gospel.
namespace Palette
{
    inline lv_color_t bgApp() { return lv_color_hex(0x201e29); }      // screen bg (mockup's dark gradient stop)
    inline lv_color_t bgPanel() { return lv_color_hex(0x2a2735); }    // list rows / ring inner arc
    inline lv_color_t bgTerminal() { return lv_color_hex(0x322f3e); } // hub bg (mockup's hub reads lighter than the screen bg)
    inline lv_color_t border() { return lv_color_hex(0x71707d); }     // outlines + secondary/unselected text
    inline lv_color_t textMuted() { return lv_color_hex(0x8f8d9b); }
    inline lv_color_t accent() { return lv_color_hex(0xc0abe9); }          // primary/selection lavender
    inline lv_color_t accentSecondary() { return lv_color_hex(0xeaae93); } // secondary warm-peach accent
    // Dark foreground for text/icons drawn on top of accent()/
    // accentSecondary() fills -- both are light pastels, so default
    // light theme text would have poor contrast on them.
    inline lv_color_t accentFg() { return lv_color_hex(0x18141f); }
    inline lv_color_t selectedFill() { return lv_color_hex(0x453a5e); } // dim accent-family fill, e.g. selected file row
    inline lv_color_t alert() { return lv_color_hex(0xc4483f); } // muted safety-red, E-Stop only
}
