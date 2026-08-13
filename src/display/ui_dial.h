#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiDialCreate();

// Registers what happens when an item is opened (a tap on it or on the
// centre hub, or a knob click) -- ui_nav owns the actual screen-navigation
// logic since it already owns the screens[] array. Going back is the
// knob long-press-to-Home convention, already screen-agnostic in
// ui_nav.cpp.
void uiDialSetHandlers(void (*onOpen)(int index));

// Knob integration -- only meaningful while the dial is the active screen.
void uiDialSelectNext();
void uiDialSelectPrev();
void uiDialOpenSelected();

// Live machine status, shown in the centre hub.
void uiDialUpdate(const FluidNCStatus &st);
