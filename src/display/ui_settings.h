#pragma once
#include <lvgl.h>
lv_obj_t *uiSettingsCreate();

// Called by ui_nav only while this screen is active.
// Rotate: steps the category ring, or scrolls an open category's controls.
void uiSettingsHandleRotate(int32_t delta);
// Click: opens the highlighted category.
void uiSettingsHandleClick();
// Back (knob long-press or the on-screen arrow). Returns true if it was
// consumed by closing an open category -- ui_nav should only leave the
// screen entirely when this returns false.
bool uiSettingsHandleBack();

// Call every loop iteration: refreshes the Wi-Fi connection status and the
// About IP/uptime -- cheap enough to run unconditionally rather than gating
// on which category is currently visible.
void uiSettingsUpdate();
