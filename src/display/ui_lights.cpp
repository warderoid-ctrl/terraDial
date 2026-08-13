#include "ui_lights.h"
#include "../led/panel_ring.h"
#include "../led/rail_leds.h"
#include "../config/settings.h"
#include "palette.h"
#include "ui_screen_shell.h"
#include "ui_widgets.h"
#include <stdio.h>

// Lights: the plotter's rail strip plus the panel's own bezel ring.
//
// Both are driven locally now. This screen used to be a remote control for
// terraPixel (a separate ESP32-C3) over HTTP, which meant every control was
// a blocking network round-trip and the screen could only ever show stale
// state. The panel mounts beside the rail, so it drives the strip directly
// -- see src/led/rail_leds.h. Every control here is immediate.
namespace
{
    lv_obj_t *lightsPanel = nullptr; // the scrollable page, for knob scrolling
    lv_obj_t *panelSlider = nullptr;
    lv_obj_t *filmSwitch = nullptr;
    lv_obj_t *railBrightSlider = nullptr;
    lv_obj_t *radiusSlider = nullptr;
    lv_obj_t *radiusLbl = nullptr;
    lv_obj_t *partyBtnLbl = nullptr;
    lv_obj_t *sleepChips[3] = {nullptr};
    lv_obj_t *sleepChipLbls[3] = {nullptr};

    const char *SLEEP_LABELS[3] = {"On", "Dim", "Off"};

    bool suppressEvents = false; // true while we're programmatically syncing widgets

    void panelSliderCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        panelRing.setBrightness((uint8_t)lv_slider_get_value(panelSlider));
    }

    void filmSwitchCb(lv_event_t *e)
    {
        (void)e;
        if (suppressEvents) return;
        Config::get().railFilmMode = lv_obj_has_state(filmSwitch, LV_STATE_CHECKED);
        Config::save();
    }

    void railBrightSliderCb(lv_event_t *e)
    {
        if (suppressEvents) return;
        Config::get().railFilmBrightness = (uint8_t)lv_slider_get_value(railBrightSlider);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) Config::save();
    }

    void radiusSliderCb(lv_event_t *e)
    {
        if (suppressEvents) return;
        float radius = lv_slider_get_value(radiusSlider) / 10.0f;
        Config::get().railCometRadius = radius;
        char buf[24];
        snprintf(buf, sizeof(buf), "Comet width: %.1f", radius);
        lv_label_set_text(radiusLbl, buf);
        if (lv_event_get_code(e) == LV_EVENT_RELEASED) Config::save();
    }

    void partyBtnCb(lv_event_t *e)
    {
        (void)e;
        railLeds.toggleParty();
        lv_label_set_text(partyBtnLbl, railLeds.party() ? "Party: ON" : "Party: OFF");
    }

    void restyleSleepChips()
    {
        for (int i = 0; i < 3; i++)
        {
            bool sel = Config::get().railSleepMode == (uint8_t)i;
            lv_obj_set_style_bg_color(sleepChips[i], sel ? Palette::accent() : Palette::bgPanel(), 0);
            lv_obj_set_style_text_color(sleepChipLbls[i], sel ? Palette::accentFg() : Palette::textMuted(), 0);
        }
    }

    void sleepChipCb(lv_event_t *e)
    {
        Config::get().railSleepMode = (uint8_t)(intptr_t)lv_event_get_user_data(e);
        Config::save();
        restyleSleepChips();
    }
}

lv_obj_t *uiLightsCreate()
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, Palette::bgApp(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *panel = uiMakePanel(scr, "LIGHTS");
    lightsPanel = panel;

    // -- rail strip --
    lv_obj_t *filmRow = uiMakeRow(panel, "Film mode");
    filmSwitch = uiMakeSwitch(filmRow, Config::get().railFilmMode);
    lv_obj_add_event_cb(filmSwitch, filmSwitchCb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *railBrightRow = uiMakeRow(panel, "Rail brightness");
    railBrightSlider = uiMakeSlider(railBrightRow, 10, 255, Config::get().railFilmBrightness);
    lv_obj_add_event_cb(railBrightSlider, railBrightSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(railBrightSlider, railBrightSliderCb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *radiusRow = uiMakeRow(panel);
    radiusLbl = lv_label_create(radiusRow);
    lv_obj_set_style_text_font(radiusLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(radiusLbl, Palette::textMuted(), 0);
    // value/10 = LEDs (1.0-8.0), stored x10 so the slider can stay integer
    radiusSlider = uiMakeSlider(radiusRow, 10, 80, (int)(Config::get().railCometRadius * 10.0f));
    lv_obj_add_event_cb(radiusSlider, radiusSliderCb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(radiusSlider, radiusSliderCb, LV_EVENT_RELEASED, NULL);
    {
        char buf[24];
        snprintf(buf, sizeof(buf), "Comet width: %.1f", Config::get().railCometRadius);
        lv_label_set_text(radiusLbl, buf);
    }

    // -- what the rail does once the panel sleeps --
    lv_obj_t *sleepRow = uiMakeRow(panel, "Rail when asleep");
    lv_obj_t *chipRow = lv_obj_create(sleepRow);
    lv_obj_set_size(chipRow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(chipRow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chipRow, 0, 0);
    lv_obj_set_style_pad_all(chipRow, 0, 0);
    lv_obj_set_style_pad_column(chipRow, 6, 0);
    lv_obj_clear_flag(chipRow, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(chipRow, LV_FLEX_FLOW_ROW);
    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *chip = lv_obj_create(chipRow);
        lv_obj_set_size(chip, 48, 26);
        lv_obj_set_style_radius(chip, 13, 0);
        lv_obj_set_style_border_width(chip, 0, 0);
        lv_obj_set_style_pad_all(chip, 0, 0);
        lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_ext_click_area(chip, 4);
        lv_obj_add_event_cb(chip, sleepChipCb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *lbl = lv_label_create(chip);
        lv_label_set_text(lbl, SLEEP_LABELS[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        sleepChips[i] = chip;
        sleepChipLbls[i] = lbl;
    }
    restyleSleepChips();

    lv_obj_t *partyBtn = uiMakeButton(panel, "Party: OFF", &partyBtnLbl);
    lv_obj_add_event_cb(partyBtn, partyBtnCb, LV_EVENT_CLICKED, NULL);

    // -- panel bezel ring (local to the display board) --
    lv_obj_t *panelRow = uiMakeRow(panel, "Panel ring brightness");
    panelSlider = uiMakeSlider(panelRow, 0, 100, panelRing.brightness());
    lv_obj_add_event_cb(panelSlider, panelSliderCb, LV_EVENT_VALUE_CHANGED, NULL);

    addBackButton(scr);
    return scr;
}

void uiLightsHandleRotate(int32_t delta)
{
    if (!lightsPanel || delta == 0) return;
    // Same step as a Settings category panel so scrolling feels identical
    // wherever the knob is scrolling a page rather than stepping a ring.
    lv_obj_scroll_by(lightsPanel, 0, -delta * 24, LV_ANIM_ON);
}

void uiLightsOnShow()
{
    // Everything is local state now, so this is a plain widget sync -- it
    // used to be a blocking HTTP round-trip to fetch terraPixel's state.
    suppressEvents = true;
    lv_slider_set_value(panelSlider, panelRing.brightness(), LV_ANIM_OFF);
    lv_slider_set_value(railBrightSlider, Config::get().railFilmBrightness, LV_ANIM_OFF);
    lv_slider_set_value(radiusSlider, (int)(Config::get().railCometRadius * 10.0f), LV_ANIM_OFF);
    if (Config::get().railFilmMode) lv_obj_add_state(filmSwitch, LV_STATE_CHECKED);
    else lv_obj_clear_state(filmSwitch, LV_STATE_CHECKED);
    lv_label_set_text(partyBtnLbl, railLeds.party() ? "Party: ON" : "Party: OFF");
    restyleSleepChips();
    suppressEvents = false;
}

void uiLightsUpdate()
{
    // Nothing to poll: the rail is driven from this firmware, so these
    // widgets are already the source of truth. Party mode is the one thing
    // that can change from outside (a [MSG:...PARTY] line in the running
    // G-code), so keep its label honest.
    if (!partyBtnLbl) return;
    lv_label_set_text(partyBtnLbl, railLeds.party() ? "Party: ON" : "Party: OFF");
}
