#pragma once

// Single source of truth for the CrowPanel 1.28" Rotary Display pin map.
// Confirmed against Elecrow's factory example firmware for this exact
// board (example/RotaryScreen_1_28_new), not guessed.

// ---- Display (GC9A01, SPI2_HOST) ----
#define PIN_LCD_SCLK 10
#define PIN_LCD_MOSI 11
#define PIN_LCD_MISO -1
#define PIN_LCD_DC 3
#define PIN_LCD_CS 9
#define PIN_LCD_RST 14
#define PIN_LCD_BACKLIGHT 46

// ---- Touch (CST816D, on Wire1) ----
#define PIN_TOUCH_SDA 6
#define PIN_TOUCH_SCL 7
#define PIN_TOUCH_INT 5
#define PIN_TOUCH_RST 13

// ---- Rotary encoder / jog wheel ----
#define PIN_ENCODER_A 45 // CLK
#define PIN_ENCODER_B 42 // DT
#define PIN_ENCODER_SW 41 // push button, active-low

// ---- Onboard LED ring (5x WS2812, GRB) ----
#define PIN_LED_RING 48
#define LED_RING_COUNT 5

// ---- terraPen rail light strip (WS2812, GRB) ----
// The panel mounts on the plotter beside the rail, so it drives the rail
// strip directly -- this replaced a separate ESP32-C3 (terraPixel) that used
// to do it over HTTP. See src/led/rail_leds.h.
//
// PIN_RAIL_LEDS must be a GPIO actually broken out on your mount. GPIO4 is
// free in this firmware and matches the pin terraPixel used, so existing
// wiring habits carry over -- but CHECK IT against the pin your connector
// exposes before wiring up.
//
// Electrical notes that matter more than the pin choice:
//   * the strip needs its own 5V feed with a common ground back to the panel
//     -- do not pull strip current through the panel's regulator
//   * WS2812s at 5V want ~3.5V for a logic high and the S3 drives 3.3V. Over
//     a short run this usually works; if the first LED misbehaves, add a
//     74AHCT125 level shifter or drop the strip to ~4.5V
#define PIN_RAIL_LEDS 4
#define RAIL_LED_COUNT 39

// ---- Power indicator LED ----
#define PIN_POWER_LED 40

// ---- Power-enable rail ----
// Confirmed from Elecrow's factory example: these two GPIOs gate a power
// rail feeding the display/backlight circuitry and must be driven HIGH
// before anything else, or the backlight stays dark no matter what's
// written to PIN_LCD_BACKLIGHT -- the example's own comments just call
// them "power pin, output current" without saying what they feed.
#define PIN_POWER_RAIL_1 1
#define PIN_POWER_RAIL_2 2

// Backlight PWM channel config (ledc)
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_PWM_FREQ 5000
#define BACKLIGHT_PWM_RESOLUTION 8

// ---- Physical mounting rotation ----
// LovyanGFX setRotation() step, 0-3 (each step = 90 deg). Set so the UI
// reads upright given how the panel is physically mounted (e.g. which side
// the cable exits). main.cpp's touchpadRead() applies a matching coordinate
// transform -- if you change this, touch has to rotate with it or taps
// land in the wrong place.
//
// 3 is a first guess at "90 deg counter-clockwise from the factory default"
// -- rotation direction conventions are easy to get backwards without the
// hardware in hand. If the display rotates the wrong way, or rotates right
// but touch is offset, try 1 instead (the other 90 deg step) and say what
// you saw.
#define DISPLAY_ROTATION 3
