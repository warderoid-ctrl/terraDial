#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiDialCreate();

// Registers what happens when an item is opened (a tap on it or on the
// centre hub, or a knob click) -- ui_nav owns the actual screen-navigation
// logic since it already owns the screens[] array. Going back is the
// knob long-press-to-Home convention, already screen-agnostic in
// ui_nav.cpp.
// onStatus is consulted first when the centre hub is tapped: it returns
// true if the machine's current state had a screen worth jumping to (a live
// job, an alarm), in which case the tap means that instead of "open the
// selected item". It is how Job Progress and Alarm Clear stay reachable
// without each holding a ring slot -- see the note at the top of
// ui_dial.cpp.
void uiDialSetHandlers(void (*onOpen)(int index), bool (*onStatus)());

// Knob integration -- only meaningful while the dial is the active screen.
void uiDialSelectNext();
void uiDialSelectPrev();
void uiDialOpenSelected();

// Live machine status, shown in the centre hub.
void uiDialUpdate(const FluidNCStatus &st);
