#pragma once
#include <lvgl.h>
lv_obj_t *uiEstopCreate();

// Fires the stop -- exposed for a knob click, in addition to the on-screen
// button's own touch handler.
void uiEstopTrigger();
