#pragma once
#include <lvgl.h>

struct ScreenShell
{
    lv_obj_t *screen;
    lv_obj_t *ring;    // thin decorative ring near the edge, caller may recolor (e.g. Jog/Files use this as their "active" cue)
    lv_obj_t *content; // circular-clipped, vertically-stacked flex container sized to the round-safe area, ready for the screen's own widgets
};

// Shared visual chrome for every sub-screen (Jog/Files/Pen/Home/Lights/
// Settings), so they read as one consistent product instead of each
// re-deriving its own layout: app-bg background, a thin ring framing the
// round panel, a small icon+title header (the icon matches the item's dial
// wedge icon, tying the two screens together visually), and a
// circular-clipped, centered content panel. Screens add their own widgets
// into `content` (or directly into `screen` if they need something outside
// the round-safe area, like the Jog/Files ring color changes).
//
// icon: an LV_SYMBOL_* placeholder, matching the same item's icon on the
// dial (see ui_dial.cpp's DIAL_ITEMS) -- not the real Lucide glyph yet.
// Includes a back-arrow button (see addBackButton() below) automatically.
ScreenShell createScreenShell(const char *title, const char *icon);

// Small back-arrow chip near the bottom edge of the round screen, tapping
// it calls UiNav::goHome() -- the same destination the knob's long-press-
// back convention already reaches, just as a visible, discoverable
// alternative for anyone who found holding the knob counter-intuitive.
// createScreenShell() adds this itself; screens that build their own root
// instead of using the shell (Jog/Files/Settings/Job Progress/E-Stop/Alarm
// Clear) call this directly on their own screen object.
void addBackButton(lv_obj_t *screen);
