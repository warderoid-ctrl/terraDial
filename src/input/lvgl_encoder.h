#pragma once
#include <lvgl.h>

// Bridges the physical jog wheel to LVGL as an ENCODER-type input device,
// so LVGL widgets that support encoder navigation (notably lv_keyboard's
// underlying button matrix) can be driven by the knob: rotate moves the
// highlighted key, click presses it.
//
// This exists because the on-screen keyboard is unusable by touch alone on
// a 240x240 round panel -- a full QWERTY map puts ~10 keys across ~240px,
// so each key is narrower than a fingertip, and the bottom rows fall near
// the curved bezel where they're hard to hit at all.
//
// The knob is normally owned by ui_nav (rotate = page the dial/carousel,
// click = open). Only ONE of them may consume it at a time, since
// jogWheel.takeRotationDelta()/takeButtonEvent() are destructive reads:
// while captured, ui_nav skips its own knob handling entirely and this
// module feeds the events to LVGL instead.
namespace LvglEncoder
{
    // Registers the LVGL indev. Call once, after lv_init() and after the
    // display driver is registered.
    void begin();

    // Routes knob input to `group` until release(). `onCancel` fires on a
    // knob long-press, giving a way out that doesn't depend on hitting a
    // small on-screen button (pass nullptr for none).
    void capture(lv_group_t *group, void (*onCancel)());
    void release();

    // ui_nav checks this to know whether to keep its hands off the knob.
    bool isCaptured();
}
