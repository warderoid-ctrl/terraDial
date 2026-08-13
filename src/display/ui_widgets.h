#pragma once
#include <lvgl.h>

// Flex-column row container used by every settings-style screen (a muted
// label above a control, e.g. a slider). Shared so screens don't each
// re-derive the same styling.
//
// labelText: pass nullptr to create the row without a label (caller adds
// its own children directly).
// outLabel: if non-null and labelText is non-null, receives the created
// label so the caller can update its text later.
lv_obj_t *uiMakeRow(lv_obj_t *parent, const char *labelText = nullptr, lv_obj_t **outLabel = nullptr);

// A full-face settings-style page: fills the screen and scrolls vertically,
// with a small accent title at the top. Used by the Settings category
// panels and the Lights screen so they're laid out identically.
//
// It deliberately fills the whole 240x240 rather than sitting in an inset
// card -- an inset left a wide dead margin and only ~142px of usable width.
// The PADDING is what keeps content inside the round bezel, and it is
// load-bearing: content spans y=46..184, where the face's half-width is
// sqrt(120^2 - 74^2) = 94px, comfortably clear of the 90px the 180px content
// column needs. Widening the content or shrinking the vertical padding will
// start clipping rows against the curve.
lv_obj_t *uiMakePanel(lv_obj_t *parent, const char *title);

// Palette-styled controls. LVGL's built-in theme paints sliders/switches in
// its own default blue-on-grey, which is why the Lights and Settings
// screens looked unstyled next to the hand-styled dial/cards -- they were
// the only places still showing stock widgets. Create controls through
// these instead of lv_slider_create/lv_switch_create directly.
lv_obj_t *uiMakeSlider(lv_obj_t *parent, int32_t min, int32_t max, int32_t value);
lv_obj_t *uiMakeSwitch(lv_obj_t *parent, bool checked);
// Full-width accent pill with a centered label. outLabel receives the label
// so callers can retitle it later.
lv_obj_t *uiMakeButton(lv_obj_t *parent, const char *text, lv_obj_t **outLabel = nullptr);
