#pragma once
#include <lvgl.h>

// Colors lifted directly from terraForge's own index.css so the plotter's
// on-machine UI reads as the same product as the desktop app. TEXT_MUTED
// and BG_TERMINAL weren't given exact values -- these two are estimates in
// the same family pending the real hex.
namespace Palette
{
    inline lv_color_t bgApp() { return lv_color_hex(0x1a1a2e); }
    inline lv_color_t bgPanel() { return lv_color_hex(0x16213e); }
    inline lv_color_t border() { return lv_color_hex(0x5c7a9e); }
    inline lv_color_t accent() { return lv_color_hex(0xe94560); }
    inline lv_color_t selectedFill() { return lv_color_hex(0x1a3a6e); }
    inline lv_color_t textMuted() { return lv_color_hex(0x7a8aa3); } // estimate
    inline lv_color_t bgTerminal() { return lv_color_hex(0x10141f); } // estimate
}
