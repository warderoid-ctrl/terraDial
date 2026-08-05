#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "pins.h"
#include "secrets.h"
#include "config/settings.h"
#include "display/lgfx_config.h"
#include "display/ui_nav.h"
#include "display/ui_dial.h"
#include "display/ui_jog.h"
#include "display/ui_files.h"
#include "display/ui_lights.h"
#include "input/encoder.h"
#include "led/panel_ring.h"
#include "net/fluidnc_client.h"
#include "net/terrapixel_client.h"
#include <CST816D.h>

// ---- terraTouch: CrowPanel round-screen control panel for terraPen ----
// See the project plan for the staged build order; this file wires
// together display/touch/encoder/LEDs, the FluidNC + terraPixel clients,
// and the backlight idle timeout.

static LGFX gfx;
static CST816D touch(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_RST, PIN_TOUCH_INT);

static const uint32_t SCREEN_W = 240;
static const uint32_t SCREEN_H = 240;

// Backlight idle-dim levels and the status-widget refresh rate -- named
// here since they live in the loop() a future feature is likely to touch.
static const uint8_t BACKLIGHT_DIMMED_PCT = 8;
static const uint8_t BACKLIGHT_FULL_PCT = 100;
static const uint32_t STATUS_UI_REFRESH_MS = 150;

static lv_disp_draw_buf_t drawBuf;
static lv_color_t *lvBuf0 = nullptr;
static lv_color_t *lvBuf1 = nullptr;

static void dispFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *colorP)
{
    if (gfx.getStartCount() > 0) gfx.endWrite();
    gfx.pushImageDMA(area->x1, area->y1,
                      area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                      (lgfx::rgb565_t *)&colorP->full);
    lv_disp_flush_ready(disp);
}

// The CST816D driver reports raw touch coordinates in the panel's native
// (unrotated) frame -- we read it directly rather than through LGFX's own
// touch abstraction, so WE have to apply the same rotation as
// gfx.setRotation(DISPLAY_ROTATION) or taps land in the wrong place versus
// what's visually displayed.
static void rotateTouchCoords(uint16_t &x, uint16_t &y)
{
#if DISPLAY_ROTATION == 1
    uint16_t nx = SCREEN_H - 1 - y;
    uint16_t ny = x;
    x = nx;
    y = ny;
#elif DISPLAY_ROTATION == 2
    x = SCREEN_W - 1 - x;
    y = SCREEN_H - 1 - y;
#elif DISPLAY_ROTATION == 3
    // Confirmed on hardware: the straightforward 90deg transform here
    // landed taps exactly 180deg opposite the wedge that was actually
    // tapped (e.g. tapping "Stop" opened "Pen", exactly across the dial) --
    // this is that transform with the 180deg error already cancelled out,
    // not a generic "rotation 3" formula.
    uint16_t nx = SCREEN_W - 1 - y;
    uint16_t ny = x;
    x = nx;
    y = ny;
#endif
    // DISPLAY_ROTATION == 0: no transform needed.
}

static void touchpadRead(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    uint16_t x, y;
    uint8_t gesture;
    bool touched = touch.getTouch(&x, &y, &gesture);
    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        rotateTouchCoords(x, y);
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    }
}

// Must happen before touch/display init: this panel gates its
// display/backlight power rail behind these two GPIOs (see pins.h).
// Without this, the backlight never lights regardless of the PWM
// value written to PIN_LCD_BACKLIGHT.
static void initPowerRails()
{
    pinMode(PIN_POWER_RAIL_1, OUTPUT);
    digitalWrite(PIN_POWER_RAIL_1, HIGH);
    pinMode(PIN_POWER_RAIL_2, OUTPUT);
    digitalWrite(PIN_POWER_RAIL_2, HIGH);

    pinMode(PIN_POWER_LED, OUTPUT);
    digitalWrite(PIN_POWER_LED, LOW);
}

static void initTouchAndDisplay()
{
    touch.begin();
    Serial.println("touch.begin() done");

    gfx.init();
    gfx.setRotation(DISPLAY_ROTATION);
    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

    lv_init();

    size_t bufSize = sizeof(lv_color_t) * SCREEN_W * SCREEN_H;
    lvBuf0 = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
    lvBuf1 = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);
    if (!lvBuf0 || !lvBuf1)
    {
        Serial.println("Failed to allocate LVGL draw buffers from PSRAM!");
    }
    lv_disp_draw_buf_init(&drawBuf, lvBuf0, lvBuf1, SCREEN_W * SCREEN_H);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = SCREEN_W;
    dispDrv.ver_res = SCREEN_H;
    dispDrv.flush_cb = dispFlush;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchpadRead;
    lv_indev_drv_register(&indevDrv);
}

static void initWifi()
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // matches terraPixel's note: WiFi sleep hurts responsiveness of the status stream
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.println("WiFi.begin() issued, connecting in background");
}

void setup()
{
    Serial.begin(115200);
    delay(300); // give the USB-CDC serial monitor time to attach before the first prints
    Serial.println("terraTouch boot");

    Config::begin();

    initPowerRails();
    initTouchAndDisplay();
    jogWheel.begin();

    UiNav::begin();

    panelRing.begin();
    panelRing.setMode(MachineMode::Boot);

    backlightInit();

    initWifi();

    Serial.println("terraTouch stage-2 bring-up ready");
}

// Backlight idle timeout: dims (not off, so the panel stays readable at a
// glance) after N seconds of no touch AND no knob activity. 0 = never.
// lv_disp_get_inactive_time() covers touch; jogWheel tracks its own
// activity separately since the encoder isn't registered as an LVGL indev.
static void updateBacklightIdle()
{
    static bool backlightDimmed = false;
    uint16_t timeoutSec = Config::get().backlightTimeoutSec;
    if (timeoutSec > 0)
    {
        uint32_t idleMs = min(lv_disp_get_inactive_time(NULL), jogWheel.msSinceActivity());
        uint32_t timeoutMs = (uint32_t)timeoutSec * 1000UL;
        if (idleMs > timeoutMs && !backlightDimmed)
        {
            backlightSet(BACKLIGHT_DIMMED_PCT);
            backlightDimmed = true;
        }
        else if (idleMs <= timeoutMs && backlightDimmed)
        {
            backlightSet(BACKLIGHT_FULL_PCT);
            backlightDimmed = false;
        }
    }
    else if (backlightDimmed)
    {
        backlightSet(BACKLIGHT_FULL_PCT);
        backlightDimmed = false;
    }
}

void loop()
{
    lv_timer_handler();
    jogWheel.update();
    UiNav::update();

    static bool fluidNcBegun = false;
    if (!fluidNcBegun && WiFi.status() == WL_CONNECTED)
    {
        Serial.print("WiFi connected, IP ");
        Serial.println(WiFi.localIP());
        // Shared by both clients below -- unique hostname, distinct from
        // FluidNC's own "terrapen" and terraPixel's "terrapen-leds".
        MDNS.begin("terratouch");
        fluidNC.begin();
        terraPixel.begin();
        fluidNcBegun = true;
    }
    if (fluidNcBegun) fluidNC.update();

    uiFilesUpdate();
    uiLightsUpdate();

    panelRing.setMode(fluidNC.status().mode);
    panelRing.update();

    static uint32_t lastStatusUiUpdate = 0;
    if (millis() - lastStatusUiUpdate > STATUS_UI_REFRESH_MS)
    {
        lastStatusUiUpdate = millis();
        uiDialUpdate(fluidNC.status());
        uiJogUpdate(fluidNC.status());
    }

    updateBacklightIdle();

    delay(5);
}
