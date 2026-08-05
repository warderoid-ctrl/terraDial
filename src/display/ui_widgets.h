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
