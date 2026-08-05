#pragma once
#include <lvgl.h>

lv_obj_t *uiFilesCreate();

// Visual feedback for whether the knob controls the file list (vs.
// top-level navigation). Becoming focused triggers a fresh SD listing.
void uiFilesSetFocused(bool focused);

// Called by ui_nav only while this screen is focused.
void uiFilesHandleRotate(int32_t delta); // move the highlighted row
void uiFilesHandleSelect();              // open Run/Delete confirm for the highlighted file

// Call every loop iteration: checks for a completed SD listing and
// rebuilds the list widget when one arrives.
void uiFilesUpdate();
