#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiDialCreate();

// Registers what happens when a wedge is opened (touch double-tap or knob
// click) / the hub is tapped (touch only -- knob long-press goes to
// Settings instead, wired separately since Settings isn't a wedge in this
// 6-item layout). ui_nav owns the actual screen-navigation logic since it
// already owns the screens[] array.
void uiDialSetHandlers(void (*onOpen)(int index), void (*onBack)());

// Knob integration -- only meaningful while the dial is the active screen.
void uiDialSelectNext();
void uiDialSelectPrev();
void uiDialOpenSelected();

// Live machine status shown on the hub's title line.
void uiDialUpdate(const FluidNCStatus &st);
