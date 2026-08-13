#pragma once
#include <lvgl.h>
#include <stddef.h>

// Full-screen radial text entry: characters sit around the rim, the knob
// moves the highlight, and the centre hub commits the highlighted key.
//
// This replaces LVGL's stock lv_keyboard for every text field in the app.
// A QWERTY map across a 240px round panel gives ~24px keys -- narrower than
// a fingertip -- and its bottom rows fall outside the visible circle
// entirely. Spreading the same characters around the rim gives each one a
// much larger angular target and, more importantly, makes the knob the
// primary input: you can enter a password without ever hitting a small key.
//
// Modal: while open it owns the knob (ui_nav routes events here and does
// nothing else) and draws on lv_layer_top() above whatever screen is
// underneath.
namespace RadialKeyboard
{
    // onAccept receives the finished text (only on the OK key). onCancel
    // fires on the knob long-press. Exactly one of them runs, then the
    // overlay closes itself.
    void open(const char *title,
              const char *initial,
              size_t maxLen,
              bool password,
              void (*onAccept)(const char *text),
              void (*onCancel)());

    bool isOpen();

    // Called by ui_nav while isOpen() -- the knob belongs to this overlay.
    void handleRotate(int32_t delta);
    void handleClick();
    void handleLongPress();
}
