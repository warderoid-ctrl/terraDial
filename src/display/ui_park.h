#pragma once
#include <lvgl.h>

// "Park for photo" -- pen up, home, then run the gantry out to the far end
// of Y so the finished plot is unobstructed for a photograph.
//
// A macro rather than three trips round the dial, because the three steps
// have to be SEQUENCED: $H is asynchronous, and G-code sent while the
// machine is still homing is rejected outright (error:9, "G-code locked out
// during alarm or homing"). Firing the moves back to back from a button
// would work only by luck and timing.
lv_obj_t *uiParkCreate();

// Starts the sequence (same as tapping the screen's confirm pill) -- wired
// to a knob click from ui_nav, matching the other confirm screens.
void uiParkTrigger();

// Drives the sequence. Must be called every loop from main.cpp, NOT from
// ui_nav's per-screen dispatch: once started, the macro has to keep
// stepping while the user wanders off to another screen, which is exactly
// what someone waiting out a 30-second homing cycle will do.
void uiParkUpdate();
