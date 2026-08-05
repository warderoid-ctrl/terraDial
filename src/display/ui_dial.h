#pragma once
#include <lvgl.h>
#include "../net/fluidnc_client.h"

lv_obj_t *uiDialCreate();

// Registers what happens when a card is opened (touch tap on the centered
// card, or a knob click) -- ui_nav owns the actual screen-navigation logic
// since it already owns the screens[] array. There's no separate "hub tap
// = back" gesture in the carousel model (unlike the old radial ring) --
// the knob long-press-to-Home convention, already screen-agnostic in
// ui_nav.cpp, covers going back from anywhere.
void uiDialSetHandlers(void (*onOpen)(int index));

// Knob integration -- only meaningful while the dial is the active screen.
void uiDialSelectNext();
void uiDialSelectPrev();
void uiDialOpenSelected();

// Live machine status shown above the carousel.
void uiDialUpdate(const FluidNCStatus &st);
