#pragma once
#include <lvgl.h>
lv_obj_t *uiSettingsCreate();

// Called by ui_nav only while this screen is active: pages the carousel.
void uiSettingsHandleRotate(int32_t delta);

// Call every loop iteration: refreshes the Wi-Fi card's connection status
// and the About card's IP/uptime -- cheap enough to run unconditionally
// rather than gating on which card is currently visible.
void uiSettingsUpdate();
