#pragma once
#include <lvgl.h>

lv_obj_t *uiLightsCreate();

// Called once by ui_nav when navigating into this screen -- pulls fresh
// terraPixel state and syncs all the widgets to it.
void uiLightsOnShow();

// Call every loop iteration: periodically refreshes terraPixel status in
// the background (connection dot, mode text) without touching slider/switch
// positions, so it doesn't fight a user mid-drag.
// Called by ui_nav only while this screen is active: scrolls the page, the
// same way the knob scrolls an open Settings category.
void uiLightsHandleRotate(int32_t delta);

void uiLightsUpdate();
