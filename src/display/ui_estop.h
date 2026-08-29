#pragma once
#include <lvgl.h>
lv_obj_t *uiEstopCreate();

// Fires the stop -- exposed for a knob click, in addition to the on-screen
// button's own touch handler.
void uiEstopTrigger();

// Times out the "STOPPED" / "NOT SENT" acknowledgement back to the armed
// face. Only meaningful while the E-Stop screen is up, so ui_nav calls it
// only then -- unlike the park macro, nothing here has to keep running once
// you have left.
void uiEstopUpdate();
