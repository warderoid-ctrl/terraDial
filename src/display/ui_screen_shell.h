#pragma once
#include <lvgl.h>

struct ScreenShell
{
    lv_obj_t *screen;
    lv_obj_t *content; // circular-clipped, vertically-stacked flex container sized to the round-safe area, ready for the screen's own widgets
};

// Shared visual chrome for every sub-screen (Jog/Files/Pen/Home/Lights/
// Settings), so they read as one consistent product instead of each
// re-deriving its own layout: app-bg background, a small icon+title header
// (the icon matches the item's dial icon, tying the two screens together
// visually), and a
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

// Small red stop pip, sitting to the left of the back button. For screens
// that can put the machine in motion under your hand: from those, reaching
// the stop otherwise meant back out, rotate the dial to E-Stop, open it --
// three deliberate actions at the moment you have least patience for them.
//
// Opens the E-Stop screen rather than stopping outright. A single stray tap
// near the bottom bezel should not soft-reset a machine mid-plot, and the
// E-Stop screen's own button is 156px of red that fires on touch-down, so
// the cost of the extra tap is small and it is the tap that makes the first
// one safe to place here.
void addEstopButton(lv_obj_t *screen);
