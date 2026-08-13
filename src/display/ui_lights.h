#pragma once
#include <lvgl.h>

lv_obj_t *uiLightsCreate();

// Called once by ui_nav when navigating into this screen: syncs every widget
// to the current settings. Cheap now that the rail is driven locally -- this
// used to be a blocking HTTP fetch from a separate controller.
void uiLightsOnShow();

// Called by ui_nav only while this screen is active: scrolls the page, the
// same way the knob scrolls an open Settings category.
void uiLightsHandleRotate(int32_t delta);

// Call while this screen is visible. Only keeps the party-mode label honest,
// since that's the one thing that can change from outside the screen (a
// [MSG:...PARTY] line in the running G-code).
void uiLightsUpdate();
