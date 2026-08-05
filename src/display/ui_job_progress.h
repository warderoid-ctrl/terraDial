#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

// Auto-navigated-to when a job starts (see ui_nav.cpp) -- not a direct
// dial destination.
lv_obj_t *uiJobProgressCreate();
void uiJobProgressUpdate(const FluidNCStatus &st);

// Toggles feed-hold/resume -- wired to both the on-screen pause button and
// a knob click, per the mockup's "click knob also pauses".
void uiJobProgressTogglePause();
