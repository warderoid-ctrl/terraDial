#pragma once
#include <lvgl.h>

// Auto-navigated-to when the machine enters Alarm state (see ui_nav.cpp) --
// also reachable directly as a dial destination, in case a user backs out
// before clearing.
lv_obj_t *uiAlarmClearCreate();

// Refreshes the body copy with FluidNC's own last error/ALARM line, so the
// screen names the actual reason instead of generic advice. Call while
// visible.
void uiAlarmClearUpdate();

// Sends $X -- wired to the on-screen button and a knob click.
void uiAlarmClearTrigger();
