#pragma once
#include <lvgl.h>

// Flex-column row container used by every settings-style screen (a muted
// label above a control, e.g. a slider). Shared so screens don't each
// re-derive the same styling.
//
// labelText: pass nullptr to create the row without a label (caller adds
// its own children directly).
// outLabel: if non-null and labelText is non-null, receives the created
// label so the caller can update its text later.
lv_obj_t *uiMakeRow(lv_obj_t *parent, const char *labelText = nullptr, lv_obj_t **outLabel = nullptr);
