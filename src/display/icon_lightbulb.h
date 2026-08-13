#pragma once
#include <lvgl.h>

// Lucide "lightbulb" (lucide.dev/icons/lightbulb), rasterised from the upstream SVG
// to a 20x20 LV_IMG_CF_ALPHA_8BIT bitmap.
//
// It's alpha-only, so it carries no colour of its own -- LVGL paints it with
// the object's `img_recolor` style. That lets the dial tint it exactly like
// the LV_SYMBOL_* text icons it sits alongside (see ui_dial.cpp), which a
// normal colour image couldn't do.
//
// Drawn 1:1. Do NOT scale it with lv_img_set_zoom: transforming an image
// whose parent also has opacity < 255 sends LVGL down an offscreen-layer
// path that proved unreliable here -- the icon intermittently vanished.
//
// Regenerate with: python tools/gen_lucide_icon.py
extern const lv_img_dsc_t iconLightbulb;
