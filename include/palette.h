#pragma once
#include <lvgl.h>

// terraForge's dark theme, ported 1:1 from that project's design tokens
// (theworkisthework/terraForge, src/renderer/src/index.css -- the `:root`
// block, which is its dark/default theme). Values are copied exactly rather
// than re-derived so the plotter's desk panel and its desktop app read as
// one product: deep navy/sea-blue surfaces with a single hot red-magenta
// accent.
//
// This replaced the earlier "TerraPen Dial UI" mockup skin (a muted
// graphite-lavender scheme). The layout still follows that mockup -- only
// the colours changed.
//
// Exact fidelity depends on the real IPS panel, not a monitor -- nudge
// these after flashing if something looks off, rather than treating the hex
// values as gospel.
namespace Palette
{
    // -- Surfaces, darkest to lightest --
    inline lv_color_t bgTerminal() { return lv_color_hex(0x0d1117); }  // --tf-bg-terminal, deepest (log/console style panes)
    inline lv_color_t bgApp() { return lv_color_hex(0x1a1a2e); }       // --tf-bg-app, the screen background
    inline lv_color_t bgPanel() { return lv_color_hex(0x16213e); }     // --tf-bg-panel
    inline lv_color_t bgSecondary() { return lv_color_hex(0x0f3460); } // --tf-bg-secondary, the "dark sea blue" raised surface: cards, chips, buttons
    inline lv_color_t bgSecondaryHover() { return lv_color_hex(0x1a4a8a); } // --tf-bg-secondary-hover

    // -- Lines and text --
    inline lv_color_t border() { return lv_color_hex(0x5c7a9e); }    // --tf-border
    inline lv_color_t text() { return lv_color_hex(0xe0e0e0); }      // --tf-text (slightly off pure white)
    inline lv_color_t textMuted() { return lv_color_hex(0x9ca3af); } // --tf-text-muted
    inline lv_color_t textFaint() { return lv_color_hex(0x818ea5); } // --tf-text-faint

    // -- Accents --
    // The red-magenta that defines terraForge's look against the navy.
    inline lv_color_t accent() { return lv_color_hex(0xe94560); }      // --tf-accent
    inline lv_color_t accentHover() { return lv_color_hex(0xc73d56); } // --tf-accent-hover
    // Foreground for text/icons on accent() fills. The accent is a
    // saturated mid-tone red, so it needs light text on top (the previous
    // lavender-pastel accent needed the opposite -- dark text).
    inline lv_color_t accentFg() { return lv_color_hex(0xffffff); }
    // Informational secondary accent -- terraForge's file-browser blue.
    // Used where something needs to stand out WITHOUT competing with the
    // red (e.g. the "pen down" status pill).
    inline lv_color_t accentSecondary() { return lv_color_hex(0x60a0ff); } // --tf-fs-blue-text

    inline lv_color_t selectedFill() { return lv_color_hex(0x1a3a6e); } // --tf-file-selected, dim blue selected-row fill

    // Deliberately a deeper, heavier red than accent(): E-Stop has to read
    // as an escalation, and the accent alone is too common elsewhere in the
    // UI (selected dial item, active chips) to carry that weight by itself.
    inline lv_color_t alert() { return lv_color_hex(0xd12b3f); }
}
