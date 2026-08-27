#pragma once
#include <lvgl.h>

// Lucide "lightbulb" (lucide.dev/icons/lightbulb), rasterised from the upstream SVG to
// LV_IMG_CF_ALPHA_8BIT bitmaps -- one per size the dial draws it at.
//
// They are alpha-only, so they carry no colour of their own: LVGL paints
// them with the object's `img_recolor` style. That lets the dial tint the
// bulb exactly like the LV_SYMBOL_* text icons it sits alongside (see
// ui_dial.cpp), which a normal colour image could not do.
//
// Each is drawn 1:1 and the dial picks a bitmap rather than resizing one.
// Do NOT scale these with lv_img_set_zoom: transforming an image whose
// parent also has opacity < 255 sends LVGL down an offscreen-layer path
// that proved unreliable here -- the icon intermittently vanished.
//
// Regenerate with: python tools/gen_lucide_icon.py
// 20x20
extern const lv_img_dsc_t iconLightbulb;
// 28x28
extern const lv_img_dsc_t iconLightbulbLarge;
