#pragma once
#include <lvgl.h>
lv_obj_t *uiPenCreate();

// Flips pen state (same effect as tapping either segment) -- wired to a
// knob click from ui_nav, per the mockup's "click knob toggles".
void uiPenToggle();

// Current pen state, for Job Progress's "pen down" status pill.
bool uiPenIsDown();
