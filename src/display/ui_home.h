#pragma once
#include <lvgl.h>

// The safety gate IS this screen -- there's no separate "press $H, then
// confirm in a popup" step. Matches the mockup (screen 07, "Home (clear
// prompt)") and mirrors ui_alarm_clear.h's shape so the two confirm-style
// screens behave identically.
lv_obj_t *uiHomeCreate();

// Homes ($X first only if alarmed) -- wired to the on-screen pill and a knob click.
void uiHomeTrigger();
