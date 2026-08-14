#pragma once
#include <lvgl.h>

// The terraPen logo (theworkisthework/terrapen-identity, TP-Logo-Animated.svg)
// rasterised to a 128x128 LV_IMG_CF_ALPHA_8BIT bitmap.
//
// Alpha-only, so it has no colour of its own -- LVGL paints it with the
// object's `img_recolor`. That's how it reads correctly on this panel: the
// source artwork is a black stroke for print, and here it's drawn in a
// palette colour on the dark background without any image editing.
//
// Drawn 1:1. Do NOT scale it with lv_img_set_zoom -- transforming an image
// whose parent also has opacity < 255 sends LVGL down an offscreen-layer
// path that proved unreliable on this board.
//
// Regenerate with: python tools/gen_logo.py
extern const lv_img_dsc_t iconLogo;
