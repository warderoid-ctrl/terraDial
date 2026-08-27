/**
 * Minimal lv_conf.h for terraDial (LVGL 8.3.x on ESP32-S3 + PSRAM).
 * Trimmed from LVGL's stock template -- only the settings this project
 * actually depends on are called out explicitly; everything else keeps
 * LVGL's built-in defaults.
 */

#if 1 /* Set it to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

/*====================
   HAL SETTINGS
 *====================*/
// Earlier attempts swung these between 10 and 30 chasing input latency, but
// the real stalls were never LVGL's refresh rate -- they were blocking
// network calls sharing the main loop (see main.cpp's networkTask, which now
// runs them on the other core). With the loop no longer blocked, 16ms
// (~60Hz) is comfortably affordable again.
#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 16
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#define LV_DPI_DEF 130

// How far a finger may travel during a press before LVGL reclassifies it as
// a scroll gesture and never emits CLICKED. The 10px default is tuned for
// phone-sized screens with a stylus-ish touch; on a 240x240 round panel a
// fingertip covers a big fraction of a button and rolls several px on a
// normal tap, so taps were being silently swallowed as scrolls (felt like
// "I pressed it and nothing happened"). 30px makes taps far more forgiving.
#define LV_INDEV_DEF_SCROLL_LIMIT 30

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
// QR codes on the About screen: a 240px round panel is no place to read a
// URL, let alone type one, but a phone camera handles a QR off the glass.
// LVGL bundles qrcodegen under extra/libs/qrcode.
#define LV_USE_QRCODE 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

/*==================
 *   FONT USAGE
 *===================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*=================
 *  THEME USAGE
 *=================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

/*==================
 *   WIDGET USAGE
 *================*/
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_BTN 1
#define LV_USE_BTNMATRIX 1
#define LV_USE_CANVAS 1
#define LV_USE_CHECKBOX 1
#define LV_USE_DROPDOWN 1
#define LV_USE_IMG 1
#define LV_USE_KEYBOARD 1
#define LV_USE_LABEL 1
#define LV_USE_LINE 1
#define LV_USE_LIST 1
#define LV_USE_MSGBOX 1
#define LV_USE_ROLLER 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TEXTAREA 1
#define LV_USE_TABLE 1

/*==================
 * EXAMPLES
 *==================*/
#define LV_BUILD_EXAMPLES 0

/*--END OF LV_CONF_H--*/
#endif /*LV_CONF_H*/
#endif /*Content enable*/
