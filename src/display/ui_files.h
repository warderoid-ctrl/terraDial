#pragma once
#include <lvgl.h>

lv_obj_t *uiFilesCreate();

// Becoming focused triggers a fresh SD listing.
void uiFilesSetFocused(bool focused);

// Called by ui_nav only while this screen is focused.
void uiFilesHandleRotate(int32_t delta);     // page the carousel
void uiFilesHandleSelect();                  // run the centered card's file directly
void uiFilesHandleDoubleClick();             // open the Run/Delete/Cancel confirm instead
                                              // (long-press stays the universal "back", so
                                              // it's not reused here)

// Call every loop iteration: checks for a completed SD listing and
// rebuilds the carousel cards when one arrives.
void uiFilesUpdate();
